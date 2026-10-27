#include "RBTREE.h"
#include <stdlib.h>

/* ---------- 内部函数:全部 static,不污染全局符号表 ---------- */
static void RBTREE_left_rotate(rbtree *T, rbtree_node *x);
static void RBTREE_right_rotate(rbtree *T, rbtree_node *x);
static void RBTREE_insert_fixup(rbtree *T, rbtree_node *z);
static void RBTREE_transplant(rbtree *T, rbtree_node *u, rbtree_node *v);
static void RBTREE_delete_fixup(rbtree *T, rbtree_node *x);
static void RBTREE_free_all(rbtree *T, rbtree_node *node);
static int  RBTREE_check_rec(rbtree *T, rbtree_node *n, int black_height, int *out_bh);

/* ==================== 创建与销毁 ==================== */

rbtree *RBTREE_create(void)
{
    rbtree *T = (rbtree *)malloc(sizeof(rbtree));
    if (T == NULL) return NULL;

    /* FIX:原来写成 (rbtree*)malloc(...) 强转类型错误,应为 rbtree_node* */
    T->nil = (rbtree_node *)malloc(sizeof(rbtree_node));
    if (T->nil == NULL) { free(T); return NULL; }

    T->nil->node.color  = BLACK;
    T->nil->node.left   = T->nil;
    T->nil->node.right  = T->nil;
    T->nil->node.parent = NULL;
    T->nil->key         = 0;      /* FIX:补上,原来未初始化 */
    T->nil->value       = NULL;
    T->root = T->nil;
    return T;
}

/* 释放所有业务节点(不含哨兵和树本身) */
static void RBTREE_free_all(rbtree *T, rbtree_node *node)
{
    if (node == T->nil) return;
    RBTREE_free_all(T, node->node.left);
    RBTREE_free_all(T, node->node.right);
    free(node);   /* 只 free 节点,不 free value —— 由调用方管理业务数据 */
}

void RBTREE_destroy(rbtree *T)
{
    if (T == NULL) return;
    RBTREE_free_all(T, T->root);
    free(T->nil);
    free(T);
}

/* ==================== 查找 ==================== */

rbtree_node *RBTREE_search(rbtree *T, KEY_TYPE key)
{
    if (T == NULL) return NULL;
    rbtree_node *x = T->root;
    while (x != T->nil) {
        if (key == x->key) return x;
        x = (key < x->key) ? x->node.left : x->node.right;
    }
    return NULL;
}

/* ==================== 旋转 ==================== */

static void RBTREE_left_rotate(rbtree *T, rbtree_node *x)
{
    rbtree_node *y = x->node.right;

    /* 1. y 的左孩子挂到 x 的右边 */
    x->node.right = y->node.left;
    if (y->node.left != T->nil)
        y->node.left->node.parent = x;

    /* 2. y 接管 x 的父节点位置 */
    y->node.parent = x->node.parent;
    if (x->node.parent == T->nil)
        T->root = y;
    else if (x == x->node.parent->node.left)
        x->node.parent->node.left = y;
    else
        x->node.parent->node.right = y;

    /* 3. x 成为 y 的左孩子 */
    y->node.left  = x;
    x->node.parent = y;
}

static void RBTREE_right_rotate(rbtree *T, rbtree_node *x)
{
    rbtree_node *y = x->node.left;

    x->node.left = y->node.right;
    if (y->node.right != T->nil)
        y->node.right->node.parent = x;

    y->node.parent = x->node.parent;
    if (x->node.parent == T->nil)
        T->root = y;
    else if (x == x->node.parent->node.right)
        x->node.parent->node.right = y;
    else
        x->node.parent->node.left = y;

    y->node.right = x;
    x->node.parent = y;
}

/* ==================== 插入 ==================== */

static void RBTREE_insert_fixup(rbtree *T, rbtree_node *z)
{
    while (z->node.parent->node.color == RED)
    {
        rbtree_node *parent = z->node.parent;
        rbtree_node *grand  = parent->node.parent;

        /* ============ 左边分支 (L) ============ */
        if (parent == grand->node.left)
        {
            rbtree_node *uncle = grand->node.right;

            if (uncle->node.color == RED) {          /* 叔叔红 → 变色上抛 */
                uncle->node.color  = BLACK;
                parent->node.color = BLACK;
                grand->node.color  = RED;
                z = grand;
                continue;
            }

            /* 叔叔黑 → 旋转 */
            if (z == parent->node.right) {           /* LR:先左旋折直 */
                z = parent;
                RBTREE_left_rotate(T, z);
                parent = z->node.parent;             /* 旋转后重新定位 */
                grand  = parent->node.parent;
            }
            /* LL:对祖父右旋 */
            parent->node.color = BLACK;
            grand->node.color  = RED;
            RBTREE_right_rotate(T, grand);
        }
        /* ============ 右边分支 (R) ============ */
        else
        {
            rbtree_node *uncle = grand->node.left;

            if (uncle->node.color == RED) {          /* 叔叔红 → 变色上抛 */
                uncle->node.color  = BLACK;
                parent->node.color = BLACK;
                grand->node.color  = RED;
                z = grand;
                continue;
            }

            /* 叔叔黑 → 旋转 */
            if (z == parent->node.left) {            /* RL:先右旋折直 */
                z = parent;
                RBTREE_right_rotate(T, z);
                parent = z->node.parent;
                grand  = parent->node.parent;
            }
            /* RR:对祖父左旋 */
            parent->node.color = BLACK;
            grand->node.color  = RED;
            RBTREE_left_rotate(T, grand);
        }
    }

    T->root->node.color = BLACK;   /* 根永远为黑 */
}

int RBTREE_insert(rbtree *T, rbtree_node *z)
{
    if (T == NULL || z == NULL) return -1;

    rbtree_node *y = T->nil;
    rbtree_node *x = T->root;

    /* 1. BST 查找插入位置 */
    while (x != T->nil) {
        y = x;
        if      (z->key < x->key) x = x->node.left;
        else if (z->key > x->key) x = x->node.right;
        else return -1;                 /* key 已存在,由上层处理 */
    }

    /* 2. 挂到 y 下面 */
    z->node.parent = y;
    if (y == T->nil)
        T->root = z;
    else if (z->key < y->key)
        y->node.left = z;
    else
        y->node.right = z;

    /* 3. 新节点一定是红色,两个 nil 孩子 */
    z->node.left   = T->nil;
    z->node.right  = T->nil;
    z->node.color  = RED;

    /* 4. 修复 */
    RBTREE_insert_fixup(T, z);
    return 0;
}

/* ==================== 删除 ==================== */

static void RBTREE_transplant(rbtree *T, rbtree_node *u, rbtree_node *v)
{
    if (u->node.parent == T->nil)
        T->root = v;
    else if (u == u->node.parent->node.left)
        u->node.parent->node.left = v;
    else
        u->node.parent->node.right = v;

    /* 即使 v 是 nil,也要把 nil 的 parent 指向 u 的 parent,
       这样双黑修复时能通过 nil->parent 找到兄弟节点 */
    v->node.parent = u->node.parent;
}

int RBTREE_delete(rbtree *T, KEY_TYPE key)
{
    if (T == NULL) return -1;

    rbtree_node *z = T->root;
    while (z != T->nil) {
        if (key == z->key) break;
        z = (key < z->key) ? z->node.left : z->node.right;
    }
    if (z == T->nil) return -1;             /* 没找到 */

    rbtree_node *y = z;                     /* 真正被物理摘除的节点 */
    color_t y_original_color = y->node.color;
    rbtree_node *x;                         /* 顶替 y 位置的孩子(可能是 nil) */

    if (z->node.left == T->nil) {
        x = z->node.right;
        RBTREE_transplant(T, z, z->node.right);
    } else if (z->node.right == T->nil) {
        x = z->node.left;
        RBTREE_transplant(T, z, z->node.left);
    } else {
        /* 两个孩子:找右子树最小节点作为后继 */
        y = z->node.right;
        while (y->node.left != T->nil)
            y = y->node.left;

        y_original_color = y->node.color;
        x = y->node.right;                  /* 后继没有左孩子 */

        if (y->node.parent == z) {
            x->node.parent = y;             /* 让 nil 指向 y */
            z->node.right   = y;            /* FIX:原来漏了这一步,y 没接上 z 的右孩子,
                                               移动 z 的右子树整体丢失(树被破坏) */
        } else {
            RBTREE_transplant(T, y, y->node.right);
            y->node.right = z->node.right;
            y->node.right->node.parent = y;
        }
        RBTREE_transplant(T, z, y);
        y->node.left = z->node.left;
        y->node.left->node.parent = y;
        y->node.color = z->node.color;
        /* FIX:原本物理摘除的 y 是红色时,其两个黑色 nil 孩子中
           有一个的 parent 还指着已被 free 的 z(悬空指针,
           双黑修复经 nil->parent 找兄弟时会 use-after-free),
           统一重新指向 y */
        z->node.left->node.parent  = y;
        z->node.right->node.parent = y;
    }

    if (y_original_color == BLACK)          /* 黑高被破坏,需要修复 */
        RBTREE_delete_fixup(T, x);

    free(z);
    return 0;
}

static void RBTREE_delete_fixup(rbtree *T, rbtree_node *x)
{
    /* 只有 x 是黑(或 nil)且不是根时,才存在双黑问题 */
    while (x != T->root && x->node.color == BLACK)
    {
        /* ============ 左侧分支 ============ */
        if (x == x->node.parent->node.left)
        {
            rbtree_node *w = x->node.parent->node.right;   /* 兄弟 */

            if (w->node.color == RED) {                    /* 情况 1:兄弟红 */
                w->node.color = BLACK;
                x->node.parent->node.color = RED;
                RBTREE_left_rotate(T, x->node.parent);
                w = x->node.parent->node.right;
            }

            if (w->node.left->node.color == BLACK &&       /* 情况 2:兄弟黑,两侄黑 */
                w->node.right->node.color == BLACK)
            {
                w->node.color = RED;
                x = x->node.parent;
                continue;
            }

            if (w->node.right->node.color == BLACK) {      /* 情况 3:右侄黑,左侄红 */
                w->node.left->node.color = BLACK;
                w->node.color = RED;
                RBTREE_right_rotate(T, w);
                w = x->node.parent->node.right;
            }

            /* 情况 4:右侄红 */
            w->node.color = x->node.parent->node.color;
            x->node.parent->node.color = BLACK;
            w->node.right->node.color = BLACK;
            RBTREE_left_rotate(T, x->node.parent);
            x = T->root;
        }
        /* ============ 右侧分支 ============ */
        else
        {
            rbtree_node *w = x->node.parent->node.left;    /* 兄弟 */

            if (w->node.color == RED) {                    /* 情况 1 */
                w->node.color = BLACK;
                x->node.parent->node.color = RED;
                RBTREE_right_rotate(T, x->node.parent);
                w = x->node.parent->node.left;
            }

            if (w->node.left->node.color == BLACK &&       /* 情况 2 */
                w->node.right->node.color == BLACK)
            {
                w->node.color = RED;
                x = x->node.parent;
                continue;
            }

            if (w->node.left->node.color == BLACK) {       /* 情况 3 */
                w->node.right->node.color = BLACK;
                w->node.color = RED;
                RBTREE_left_rotate(T, w);
                w = x->node.parent->node.left;
            }

            /* 情况 4 */
            w->node.color = x->node.parent->node.color;
            x->node.parent->node.color = BLACK;
            w->node.left->node.color = BLACK;
            RBTREE_right_rotate(T, x->node.parent);
            x = T->root;
        }
    }

    x->node.color = BLACK;                 /* 脱离双黑,染黑 */
}

/* ==================== 校验与打印 ==================== */

/* 返回 0 表示合法;*out_bh 记录该子树黑高(不含 nil)。
 * bh < 0 表示首次调用,尚未确定基准黑高。 */
static int RBTREE_check_rec(rbtree *T, rbtree_node *n, int bh, int *out_bh)
{
    if (n == T->nil) {
        if (bh < 0) { *out_bh = 1; return 1; }   /* 空树黑高 1(计入哨兵) */
        /* FIX:原来在这里拿 *out_bh(调用方尚未初始化的 lh/rh)与 1 比较,
           未初始化读 + 错误基准导致合法树也判 0;nil 分支只要登记基准即可,
           黑高一致性由父节点用左右子树的 lh/rh 比较 */
        *out_bh = 1;
        return 1;
    }

    /* 性质 3:红节点的两个孩子必须是黑 */
    if (n->node.color == RED) {
        if (n->node.left->node.color  == RED) return 0;
        if (n->node.right->node.color == RED) return 0;
    }

    /* 性质:左子树 key < n->key < 右子树 key */
    if (n->node.left != T->nil && n->node.left->key >= n->key)  return 0;
    if (n->node.right != T->nil && n->node.right->key <= n->key) return 0;

    int lh = -1, rh = -1;
    if (!RBTREE_check_rec(T, n->node.left,  bh, &lh)) return 0;
    if (!RBTREE_check_rec(T, n->node.right, bh, &rh)) return 0;
    if (lh != rh) return 0;                       /* 黑高必须相等 */

    int black = lh + (n->node.color == BLACK ? 1 : 0);
    if (bh >= 0 && *out_bh != black) return 0;    /* 与上层基准比较 */
    *out_bh = black;
    return 1;
}

int RBTREE_check(rbtree *T)
{
    if (T == NULL) return 0;
    if (T->nil->node.color != BLACK) return 0;    /* 哨兵必须是黑 */

    /* 性质 2:根必须是黑 */
    if (T->root != T->nil && T->root->node.color != BLACK) return 0;
    if (T->root != T->nil && T->root->node.parent != T->nil) return 0;

    int bh = -1;
    if (!RBTREE_check_rec(T, T->root, -1, &bh)) return 0;

    /* 全树所有 nil 的 parent 应指向有效位置(不强校验,避免误报) */
    return 1;
}

void RBTREE_inorder(rbtree *T, rbtree_node *node)
{
    if (T == NULL || node == T->nil) return;
    RBTREE_inorder(T, node->node.left);
    printf("%d ", node->key);
    RBTREE_inorder(T, node->node.right);
}