#include "skiplist.h"

#include <stdlib.h>
#include <time.h>

/* ================== 内部工具 ================== */

/* 随机层数:第 i 层(0 起)以 p 晋升到第 i+1 层,期望高度 = 1/(1-p)。
 * p = 0.5 时期望 2,生成的树形结构"平均"平衡 —— 这就是跳表的精髓。 */
static int skiplist_random_level(const skiplist *s)
{
    int lvl = 0;
    /* 用定点 q5 避免每层调用 rand()/double(略快,也更易移植) */
    int threshold = (int)(s->p * 32.0);          /* 32 == 2^5 */
    while (lvl + 1 < s->max_level && (rand() & 31) < threshold)   //&31   31=11111   截取最后五位
        lvl++;
    return lvl;                                   /* 0 ~ max_level-1 */
}

/* 分配一个带 level 层 forward 的新节点 */
static skiplist_node *skiplist_new_node(int level, KEY_TYPE key, void *value)
{
    skiplist_node *n = (skiplist_node *)malloc(sizeof(skiplist_node));
    if (!n) return NULL;
    n->forward = (skiplist_node **)calloc((size_t)level + 1, sizeof(skiplist_node *));
    if (!n->forward) { free(n); return NULL; }
    n->key   = key;
    n->value = value;
    n->level = level;
    return n;
}

/* 核心定位:update[i] = 第 i 层上"最后一个 key < target 的节点"
 * (找不到就是 head)。定位完成后:
 *   update[0]->forward[0] 就是第一个 key >= target 的节点。
 * 每层从右上角向左下方走,是跳表 O(log n) 的来源。 */
static void skiplist_find_prev(const skiplist *s, KEY_TYPE key,
                               skiplist_node **update)
{
    skiplist_node *x = s->head;
    int i;
    for (i = s->cur_level; i >= 0; i--) {
        while (x->forward[i] && x->forward[i]->key < key)
            x = x->forward[i];
        update[i] = x;                          /* 记录本层的落点 */
    }
}

/* ================== 对外接口 ================== */

skiplist *skiplist_create(int max_level)
{
    if (max_level <= 0) max_level = SKIPLIST_MAX_LEVEL;
    if (max_level > SKIPLIST_MAX_LEVEL) max_level = SKIPLIST_MAX_LEVEL;

    skiplist *s = (skiplist *)calloc(1, sizeof(skiplist));
    if (!s) return NULL;

    s->head = skiplist_new_node(max_level - 1, 0, NULL); /* 头结点占满所有层 */
    if (!s->head) { free(s); return NULL; }

    s->max_level = max_level;
    s->cur_level = 0;            /* 初始只有第 0 层 */
    s->size      = 0;
    s->p         = (double)SKIPLIST_P_Q5_DEFAULT / 32.0;
    s->last_error = SKIPLIST_OK;

    srand((unsigned)time(NULL)); /* 每次进程启动一个随机种子即可 */
    return s;
}

static void skiplist_free_nodes(skiplist *s, skiplist_val_free_fn free_value)
{
    skiplist_node *x = s->head->forward[0];
    while (x) {
        skiplist_node *next = x->forward[0];
        if (free_value) free_value(x->value);
        free(x->forward);
        free(x);
        x = next;
    }
    int i;
    for (i = 0; i <= s->max_level - 1; i++)
        s->head->forward[i] = NULL;           /* 头结点各层清空 */
    s->cur_level = 0;
    s->size = 0;
}

void skiplist_destroy(skiplist *s, skiplist_val_free_fn free_value)
{
    if (!s) return;
    skiplist_free_nodes(s, free_value);
    free(s->head->forward);
    free(s->head);
    free(s);
}

void skiplist_clear(skiplist *s, skiplist_val_free_fn free_value)
{
    if (!s) return;
    skiplist_free_nodes(s, free_value);
}

int skiplist_insert(skiplist *s, KEY_TYPE key, void *value)
{
    if (!s) { /* 无法记录,直接返回 */ return -1; }

    skiplist_node **update =
        (skiplist_node **)malloc(sizeof(skiplist_node *) * (size_t)s->max_level);
    if (!update) { s->last_error = SKIPLIST_ERR_OOM; return -1; }

    skiplist_find_prev(s, key, update);

    /* 已存在 → 更新(跳表通常把重复 key 视作覆盖) */
    skiplist_node *x = update[0]->forward[0];
    if (x && x->key == key) {
        x->value = value;                      /* 旧值不释放,归调用方管 */
        s->last_error = SKIPLIST_OK;
        free(update);
        return 0;
    }

    /* 掷骰子决定新节点层数 */
    int new_lvl = skiplist_random_level(s);
    skiplist_node *nn = skiplist_new_node(new_lvl, key, value);
    if (!nn) { free(update); s->last_error = SKIPLIST_ERR_OOM; return -1; }

    /* 若新节点比现有最高层还高,把上层的落点都指到头结点 */
    int i;
    if (new_lvl > s->cur_level) {
        for (i = s->cur_level + 1; i <= new_lvl; i++) update[i] = s->head;
        s->cur_level = new_lvl;
    }

    /* 逐层缝进链表:nn 插在 update[i] 之后 */
    for (i = 0; i <= new_lvl; i++) {
        nn->forward[i]        = update[i]->forward[i];
        update[i]->forward[i] = nn;
    }

    free(update);
    s->size++;
    s->last_error = SKIPLIST_OK;
    return 1;
}

int skiplist_erase(skiplist *s, KEY_TYPE key)
{
    if (!s) return 0;

    skiplist_node **update =
        (skiplist_node **)malloc(sizeof(skiplist_node *) * (size_t)s->max_level);
    if (!update) { s->last_error = SKIPLIST_ERR_OOM; return 0; }

    skiplist_find_prev(s, key, update);
    skiplist_node *x = update[0]->forward[0];

    if (!x || x->key != key) {                 /* 不存在 */
        free(update);
        s->last_error = SKIPLIST_ERR_NOTFOUND;
        return 0;
    }

    /* 把 x 从每一层摘掉(只有真的指向 x 的层才动) */
    int i;
    for (i = 0; i <= x->level; i++)
        if (update[i]->forward[i] == x)
            update[i]->forward[i] = x->forward[i];

    /* 删完后若最高层空了,把 cur_level 压下来 */
    while (s->cur_level > 0 && s->head->forward[s->cur_level] == NULL)
        s->cur_level--;

    free(x->forward);
    free(x);
    free(update);
    s->size--;
    s->last_error = SKIPLIST_OK;
    return 1;
}

void *skiplist_get(skiplist *s, KEY_TYPE key)   /* FIX:去 const —— 内部要写 last_error */
{
    if (!s) return NULL;
    skiplist_node *x = s->head;
    int i;
    for (i = s->cur_level; i >= 0; i--) {
        while (x->forward[i] && x->forward[i]->key < key)
            x = x->forward[i];
    }
    x = x->forward[0];                          /* 第 0 层的候选即结果 */
    if (x && x->key == key) return x->value;
    s->last_error = SKIPLIST_ERR_NOTFOUND;
    return NULL;
}

int skiplist_contains(const skiplist *s, KEY_TYPE key)
{
    if (!s) return 0;
    skiplist_node *x = s->head;
    int i;
    for (i = s->cur_level; i >= 0; i--)
        while (x->forward[i] && x->forward[i]->key < key)
            x = x->forward[i];
    x = x->forward[0];
    return (x && x->key == key) ? 1 : 0;
}

size_t skiplist_size(const skiplist *s) { return s ? s->size : 0; }
/* 注:skiplist_empty/first/next/foreach/last_error 都是纯读取,保持 const;
 * 只有会写 last_error 的 get/range 被改为非 const。 */

int skiplist_empty(const skiplist *s) { return (!s || s->size == 0) ? 1 : 0; }

skiplist_node *skiplist_first(const skiplist *s)
{
    if (!s) return NULL;
    return s->head->forward[0];
}

skiplist_node *skiplist_next(const skiplist *s, skiplist_node *node)
{
    if (!s || !node) return NULL;
    return node->forward[0];
}

int skiplist_range(skiplist *s, KEY_TYPE low, KEY_TYPE high,   /* FIX:去 const */
                   skiplist_iter_fn fn, void *userdata)
{
    if (!s || !fn) { if (s) s->last_error = SKIPLIST_ERR_PARAM; return -1; }
    if (low > high) { s->last_error = SKIPLIST_ERR_RANGE; return -1; }

    /* 从 low 出发:逐层跳到 < low 的最后位置,再落到第一个 >= low 的节点 */
    skiplist_node *x = s->head;
    int i;
    for (i = s->cur_level; i >= 0; i--)
        while (x->forward[i] && x->forward[i]->key < low)
            x = x->forward[i];
    x = x->forward[0];

    while (x && x->key <= high) {
        int r = fn(x->key, x->value, userdata);
        if (r) return r;                        /* 回调要求终止 */
        x = x->forward[0];
    }
    return 0;
}

int skiplist_foreach(const skiplist *s, skiplist_iter_fn fn, void *userdata)
{
    if (!s || !fn) return 0;
    skiplist_node *x = s->head->forward[0];
    while (x) {
        int r = fn(x->key, x->value, userdata);
        if (r) return r;
        x = x->forward[0];
    }
    return 0;
}

int skiplist_set_p(skiplist *s, int p_q5)
{
    if (!s || p_q5 < 1 || p_q5 > 32) {
        if (s) s->last_error = SKIPLIST_ERR_PARAM;
        return -1;
    }
    s->p = (double)p_q5 / 32.0;
    return 0;
}

int skiplist_last_error(const skiplist *s)
{
    return s ? s->last_error : SKIPLIST_ERR_PARAM;
}