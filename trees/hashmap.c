#include "hashmap.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ================== 内部常量与类型(不进头文件) ================== */

#define HM_EMPTY   0   /* 空桶 */
#define HM_USED    1   /* 已占用 */
#define HM_DELETED 2   /* 墓碑:删除后留位,避免破坏探测链 */

#define HM_DEFAULT_CAP 16u   /* 默认初始容量 */

/* 每个桶:key + value + 状态标记 */
typedef struct hm_bucket {
    KEY_TYPE key;
    void    *value;
    unsigned char state;
} hm_bucket;

/* 表本体:对外只以不完整类型出现 */
struct hashmap {
    hm_bucket *buckets;
    size_t     cap;        /* 桶数量,恒为 2 的幂 */
    size_t     size;       /* 有效键值对个数(不含墓碑) */
    size_t     used;       /* 已用桶个数(含墓碑),used/cap 决定何时扩容 */
    int        last_error;
};

/* ================== 内部工具 ================== */

/* cap 向上取 2 的幂;0 → 1 */
static size_t hm_next_pow2(size_t v)
{
    if (v == 0) return 1;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
#if SIZE_MAX > 0xFFFFFFFFu
    v |= v >> 32;
#endif
    return v + 1;
}

/* 整数键的混合哈希(近似 Fibonacci 高位乘),降低连续 key 的聚集 */
static size_t hm_hash(KEY_TYPE key, size_t cap)
{
    unsigned long long k = (unsigned long long)(long long)key;
    k *= 0x9E3779B97F4A7C15ULL;          /* 黄金比例常数 */
    return (size_t)(k >> 32) & (cap - 1); /* cap 为 2 的幂,直接取低位 */
}

/* 查找 key 所在桶下标;未找到返回 (size_t)-1。
 * *out_slot 可为 NULL;非 NULL 时返回"应插入位置"(第一个 EMPTY/DELETED)。 */
static size_t hm_find(const hashmap *m, KEY_TYPE key, hm_bucket **out_slot)
{
    size_t mask = m->cap - 1;
    size_t i    = hm_hash(key, m->cap);
    size_t first_free = (size_t)-1;
    size_t steps = 0;

    while (steps <= mask) {               /* 最多走 cap 步,防死循环 */
        hm_bucket *b = &m->buckets[i];
        if (b->state == HM_EMPTY) {
            if (out_slot && first_free == (size_t)-1) first_free = i;
            break;                        /* 探测链中断,必然不存在 */
        }
        if (b->state == HM_DELETED) {
            if (out_slot && first_free == (size_t)-1) first_free = i;
        } else if (b->key == key) {
            if (out_slot) *out_slot = b;
            return i;
        }
        i = (i + 1) & mask;               /* 线性探测(回绕) */
        steps++;
    }
    if (out_slot)
        *out_slot = (first_free == (size_t)-1) ? NULL : &m->buckets[first_free];
    return (size_t)-1;
}

/* 扩容(或收缩)到 new_cap(2 的幂)并整体 rehash:墓碑在此被回收 */
static int hm_resize(hashmap *m, size_t new_cap)
{
    hm_bucket *nb = (hm_bucket *)calloc(new_cap, sizeof(hm_bucket));
    if (!nb) { m->last_error = HASHMAP_ERR_OOM; return -1; }

    hm_bucket *old = m->buckets;
    size_t     old_cap = m->cap;
    size_t     mask = new_cap - 1;
    size_t     i;

    for (i = 0; i < old_cap; i++) {
        if (old[i].state != HM_USED) continue;
        size_t j = hm_hash(old[i].key, new_cap);
        while (nb[j].state == HM_EMPTY)   /* 新表无墓碑,只可能撞 EMPTY */
            j = (j + 1) & mask;
        nb[j] = old[i];
    }

    free(old);
    m->buckets = nb;
    m->cap     = new_cap;
    m->used    = m->size;                 /* rehash 后墓碑全部消失 */
    return 0;
}

/* 负载因子超 0.7 扩容;失败时不致命(调用方可继续用旧表) */
static int hm_maybe_grow(hashmap *m)
{
    if (m->used * 10 < m->cap * 7) return 0;
    size_t want = m->cap * 2;
    if (want < m->cap) { m->last_error = HASHMAP_ERR_OOM; return -1; } /* 溢出 */
    return hm_resize(m, want);
}

/* ================== 对外接口 ================== */

hashmap *hashmap_create(size_t initial_cap)
{
    hashmap *m = (hashmap *)calloc(1, sizeof(hashmap));
    if (!m) return NULL;

    size_t cap = hm_next_pow2(initial_cap ? initial_cap : HM_DEFAULT_CAP);
    m->buckets = (hm_bucket *)calloc(cap, sizeof(hm_bucket));
    if (!m->buckets) { free(m); return NULL; }

    m->cap = cap;
    m->size = m->used = 0;
    m->last_error = HASHMAP_OK;
    return m;
}

void hashmap_destroy(hashmap *m, hashmap_val_free_fn free_value)
{
    if (!m) return;
    if (free_value) {
        size_t i;
        for (i = 0; i < m->cap; i++)
            if (m->buckets[i].state == HM_USED) free_value(m->buckets[i].value);
    }
    free(m->buckets);
    free(m);
}

void hashmap_clear(hashmap *m, hashmap_val_free_fn free_value)
{
    if (!m) return;
    size_t i;
    for (i = 0; i < m->cap; i++) {
        if (m->buckets[i].state == HM_USED && free_value)
            free_value(m->buckets[i].value);
        m->buckets[i].state = HM_EMPTY;
        m->buckets[i].value = NULL;
    }
    m->size = 0;
    m->used = 0;        /* FIX:原来漏了 used,清空后 used 仍是旧值,
                           下次 put 会误判负载已满而盲目扩容,且 used 与实际桶占用不符 */
    m->last_error = HASHMAP_OK;
}

int hashmap_put(hashmap *m, KEY_TYPE key, void *value)
{
    if (!m) return -1;

    hm_bucket *slot = NULL;
    hm_find(m, key, &slot);

    if (slot && slot->key == key && slot->state == HM_USED) {
        slot->value = value;              /* 更新已有键 */
        m->last_error = HASHMAP_OK;
        return 0;
    }

    if (!slot) {                          /* 表被删满占满,先扩容再重找 */
        /* FIX:原来不区分 OOM 和"表满",可能在 OOM 时误记成 FULL */
        if (hm_maybe_grow(m) != 0) return -1;
        hm_find(m, key, &slot);
        if (!slot) { m->last_error = HASHMAP_ERR_FULL; return -1; }  /* 理论上不可达 */
    }

    int reused_tombstone = (slot->state == HM_DELETED);

    /* FIX:先探测是否需要扩容再写入(原来先写后扩,扩到一半失败时
       这次插入既"已生效"又返回 -1,与头文件"分配失败,操作未生效"矛盾) */
    if (!reused_tombstone && hm_maybe_grow(m) != 0) return -1;  /* 原表未被改动 */

    slot->key    = key;
    slot->value  = value;
    slot->state  = HM_USED;
    m->size++;
    if (!reused_tombstone) m->used++;     /* 复用墓碑不增加 used */

    m->last_error = HASHMAP_OK;
    return 1;
}

int hashmap_remove(hashmap *m, KEY_TYPE key)
{
    if (!m) return 0;
    hm_bucket *slot = NULL;
    hm_find(m, key, &slot);
    if (!slot || slot->state != HM_USED) {
        m->last_error = HASHMAP_ERR_NOTFOUND;
        return 0;
    }
    slot->state = HM_DELETED;             /* 墓碑保留,探测链不断裂 */
    slot->value = NULL;
    m->size--;
    m->last_error = HASHMAP_OK;
    return 1;
}

void *hashmap_get(const hashmap *m, KEY_TYPE key)
{
    if (!m) return NULL;
    hm_bucket *slot = NULL;
    hm_find(m, key, &slot);
    return (slot && slot->state == HM_USED) ? slot->value : NULL;
}

int hashmap_contains(const hashmap *m, KEY_TYPE key)
{
    if (!m) return 0;
    hm_bucket *slot = NULL;
    hm_find(m, key, &slot);
    return (slot && slot->state == HM_USED) ? 1 : 0;
}

size_t hashmap_size(const hashmap *m) { return m ? m->size : 0; }

int hashmap_empty(const hashmap *m) { return (!m || m->size == 0) ? 1 : 0; }

int hashmap_foreach(const hashmap *m, hashmap_iter_fn fn, void *userdata)
{
    if (!m || !fn) return 0;
    size_t i;
    for (i = 0; i < m->cap; i++) {
        if (m->buckets[i].state != HM_USED) continue;
        int r = fn(m->buckets[i].key, m->buckets[i].value, userdata);
        if (r) return r;                  /* 回调要求终止 */
    }
    return 0;
}

size_t hashmap_keys(const hashmap *m, KEY_TYPE *out_keys)
{
    if (!m || !out_keys) return 0;
    size_t n = 0, i;
    for (i = 0; i < m->cap; i++)
        if (m->buckets[i].state == HM_USED) out_keys[n++] = m->buckets[i].key;
    return n;
}

int hashmap_reserve(hashmap *m, size_t n)
{
    if (!m) return -1;
    /* 需要的桶数:按负载因子 0.7 反推,再向上取 2 的幂 */
    size_t need = (size_t)((double)n / 0.7) + 1;
    size_t want = hm_next_pow2(need);
    if (want <= m->cap) { m->last_error = HASHMAP_OK; return 0; }
    return hm_resize(m, want);
}

int hashmap_last_error(const hashmap *m)
{
    return m ? m->last_error : HASHMAP_ERR_NOTFOUND;
}