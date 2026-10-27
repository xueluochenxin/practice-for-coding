#ifndef SKIPLIST_H
#define SKIPLIST_H

/* ============================================================================
 * skiplist.h —— 跳表(Skip List)库
 *
 * 跳表是什么:一种"概率平衡"的有序多层链表,用空间换时间,
 * 在平均 O(log n) 内完成查找/插入/删除,是 STL std::map(std::set) 红黑树的
 * 简单等价替代(Redis 的 zset 也用它)。
 *
 * 设计要点:
 *   - 键:   int(可在 #include 前 #define KEY_TYPE 覆盖)
 *   - 值:   void*,由调用方分配与释放,本库只存指针
 *   - 层数:   期望最高 32 层(支持 2^32 量级),每层晋升概率 1/2
 *   - 结构:  带"头结点"(不存 key) + NULL 哨兵尾(每层 right 为 NULL 即终止)
 *   - 排序:  key 从小到大;重复 key 视为"更新",返回 0
 *
 * 用法示例:
 *   skiplist *s = skiplist_create(0);        // 0 = 默认最大层数 32
 *   int *v = malloc(sizeof *v); *v = 42;
 *   skiplist_insert(s, 100, v);              // 成功返回 1
 *   int *got = skiplist_get(s, 100);         // 不存在返回 NULL
 *   if (skiplist_erase(s, 100)) free(got);   // erase 返回 1 表示删除成功
 *   skiplist_destroy(s, NULL);              // 第二参数为 value 释放函数
 *
 * 对外提供的接口(全部 skiplist_ 前缀,可与 AVL/RBTREE/hashmap 同时使用):
 *   创建/销毁 : skiplist_create, skiplist_destroy, skiplist_clear
 *   增/删/改  : skiplist_insert, skiplist_erase
 *   查/读     : skiplist_get, skiplist_contains, skiplist_size, skiplist_empty
 *   有序范围  : skiplist_first, skiplist_next, skiplist_range(区间扫描)
 *   遍历      : skiplist_foreach
 *   扩展      : skiplist_set_p
 *   错误处理  : skiplist_last_error
 * ==========================================================================*/

#include <stddef.h>

/* ---------- 可选:在 include 前定义 KEY_TYPE 覆盖键类型 ---------- */
#ifndef KEY_TYPE
typedef int KEY_TYPE;
#endif

/* 表状态:供 skiplist_last_error() 返回 */
#define SKIPLIST_OK            0  /* 上次操作成功 */
#define SKIPLIST_ERR_OOM       1  /* 内存分配失败 */
#define SKIPLIST_ERR_RANGE     2  /* 区间参数非法(low > high) */
#define SKIPLIST_ERR_NOTFOUND  3  /* 键不存在 */
#define SKIPLIST_ERR_PARAM     4  /* 参数无效(NULL 或 p 越界) */

/* 晋升概率 p 的定点数表示:p_q5 = p * 32(p ∈ [1,32])。
 * 默认 SKIPLIST_P_Q5_DEFAULT = 16,即 p = 0.5。 */
#define SKIPLIST_P_Q5_DEFAULT 16
#define SKIPLIST_MAX_LEVEL     32   /* 最大层数上限 */

/* 遍历/区间回调:返回 0 继续,非 0 提前终止 */
typedef int (*skiplist_iter_fn)(KEY_TYPE key, void *value, void *userdata);

/* value 释放回调:destroy/clear 时对每个 value 调用(可 NULL) */
typedef void (*skiplist_val_free_fn)(void *value);

/* 有序节点(头结点的 key/value 恒为 0/NULL,勿使用) */
typedef struct skiplist_node {
    KEY_TYPE key;
    void    *value;
    struct skiplist_node **forward;  /* forward[i] = 第 i 层的下一个节点 */
    int      level;                  /* 该节点实际拥有的层数 */
} skiplist_node;

/* 对外暴露的跳表结构体(字段视为内部实现,请勿直接修改) */
typedef struct skiplist {
    skiplist_node *head;             /* 头结点:永远存在,不存数据 */
    int            max_level;        /* 当前允许的最大层数 */
    int            cur_level;        /* 目前实际用到的最高层(0 起) */
    size_t         size;             /* 元素个数 */
    int            last_error;
    double         p;                /* 晋升概率,默认 0.5 */
} skiplist;

/* ======================= 对外接口 ======================= */

/* --- 创建 / 销毁 --- */

/* 创建跳表。max_level 传 0 使用默认 SKIPLIST_MAX_LEVEL(32);
 * 传入值会被夹到 [1, SKIPLIST_MAX_LEVEL]。失败返回 NULL。 */
skiplist *skiplist_create(int max_level);

/* 销毁整张表并释放全部节点。
 * free_value 非 NULL 时对每个仍存在的 value 调用一次;传 NULL 表示库不碰 value。 */
void skiplist_destroy(skiplist *s, skiplist_val_free_fn free_value);

/* 清空所有节点(保留结构,可继续插入)。free_value 语义同 destroy。 */
void skiplist_clear(skiplist *s, skiplist_val_free_fn free_value);

/* --- 增 / 删 / 改 --- */

/* 插入 (key, value)。value 只存指针,所有权归调用方。
 * 返回 1 插入成功;0 key 已存在(执行更新,新值覆盖旧值,旧值不释放);
 * -1 内存分配失败(见 skiplist_last_error)。 */
int skiplist_insert(skiplist *s, KEY_TYPE key, void *value);

/* 删除 key。成功返回 1;不存在返回 0。只 free 节点,不 free value。 */
int skiplist_erase(skiplist *s, KEY_TYPE key);

/* --- 查 / 读 --- */

/* 取 key 对应的值;不存在返回 NULL(value 为 NULL 时用 contains 区分)。 */
/* FIX:去掉 const —— 未找到时实现会写 s->last_error(与 skiplist.c 保持一致) */
void *skiplist_get(skiplist *s, KEY_TYPE key);

/* key 是否存在,返回 1 / 0。 */
int skiplist_contains(const skiplist *s, KEY_TYPE key);

/* 元素个数。 */
size_t skiplist_size(const skiplist *s);

/* 空表返回 1。 */
int skiplist_empty(const skiplist *s);

/* --- 有序范围(跳表相对普通哈希表的核心优势) --- */

/* 返回 key 最小的节点;空表返回 NULL。头结点不算节点。 */
skiplist_node *skiplist_first(const skiplist *s);

/* 返回严格大于 node->key 的下一个节点;越界返回 NULL。
 * 注意:遍历期间禁止插入/删除(会使 node 悬空)。 */
skiplist_node *skiplist_next(const skiplist *s, skiplist_node *node);

/* 区间扫描 [low, high] 闭区间,按 key 升序回调 fn。
 * fn 返回非 0 提前终止,并作为 skiplist_last_error 的 NOTFOUND 之外的返回值传出。
 * low > high 返回 SKIPLIST_ERR_RANGE。返回 0 表示全部回调完成。 */
/* FIX:去掉 const —— 参数非法时实现会写 s->last_error(与 skiplist.c 保持一致) */
int skiplist_range(skiplist *s, KEY_TYPE low, KEY_TYPE high,
                   skiplist_iter_fn fn, void *userdata);

/* --- 遍历 --- */

/* 按 key 升序对每个 (key, value) 调用 fn;fn 返回非 0 提前结束。 */
int skiplist_foreach(const skiplist *s, skiplist_iter_fn fn, void *userdata);

/* --- 扩展 --- */

/* 修改晋升概率 p(q5 = p*32,范围 [1,32])。只影响之后新节点的层数,
 * 已有节点层数不变。成功 0,参数越界 -1。 */
int skiplist_set_p(skiplist *s, int p_q5);

/* --- 错误处理 --- */

/* 返回上一次操作的状态码(SKIPLIST_OK / OOM / RANGE / NOTFOUND / PARAM)。 */
int skiplist_last_error(const skiplist *s);

#endif /* SKIPLIST_H */