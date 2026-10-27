#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "AVL.h"

/* ================= 内部工具(不进头文件) ================= */
static int imax(int a, int b) { return a > b ? a : b; }

static void avl_update_height(avl_node *x)
{
    if (x == NULL) return;
    x->height = imax(avl_height(x->left), avl_height(x->right)) + 1;
}

/* ================= 对外查询/工具 ================= */

int avl_height(const avl_node *x)
{
    return x ? x->height : 0;
}

int avl_balance(const avl_node *x)
{
    return x ? avl_height(x->left) - avl_height(x->right) : 0;
}

avl_node *avl_search(avl_node *root, int key)
{
    while (root) {
        if (root->key == key) return root;
        root = (root->key > key) ? root->left : root->right;
    }
    return NULL;
}

avl_node *avl_min(avl_node *root)
{
    if (root) while (root->left) root = root->left;
    return root;
}

avl_node *avl_max(avl_node *root)
{
    if (root) while (root->right) root = root->right;
    return root;
}

/* ================= 旋转 ================= */

static avl_node *rotate_right(avl_node *x)
{
    if (x == NULL || x->left == NULL) return x;   /* 防御:缺左孩子不旋转 */

    avl_node *y = x->left;
    avl_node *z = y->right;

    y->right = x;
    x->left  = z;

    avl_update_height(x);   /* 先更新下沉的 x,再更新上浮的 y */
    avl_update_height(y);
    return y;
}

static avl_node *rotate_left(avl_node *x)
{
    if (x == NULL || x->right == NULL) return x;  /* 防御:缺右孩子不旋转 */

    avl_node *y = x->right;
    avl_node *z = y->left;

    y->left  = x;
    x->right = z;

    avl_update_height(x);
    avl_update_height(y);
    return y;
}

/* 修正后的核心:任何失衡 (|bf| > 1) 都必须处理,不能只认 ±1 */
static avl_node *rebalance(avl_node *n)
{
    if (n == NULL) return NULL;

    avl_update_height(n);
    int bf = avl_balance(n);

    if (bf > 1) {                       /* 左边高 */
        if (avl_balance(n->left) >= 0) {   /* LL → 右旋 */
            return rotate_right(n);
        } else {                           /* LR → 左孩子左旋,再右旋 */
            n->left = rotate_left(n->left);
            return rotate_right(n);
        }
    }

    if (bf < -1) {                      /* 右边高 —— 原代码漏了 0 和 ±2 的情况 */
        if (avl_balance(n->right) <= 0) {   /* RR → 左旋 */  /* FIX:原来是 == -1 */
            return rotate_left(n);
        } else {                            /* RL → 右孩子右旋,再左旋 */ /* FIX:原来是 == 1 */
            n->right = rotate_right(n->right);
            return rotate_left(n);
        }
    }

    return n;
}

/* ================= 插入 ================= */

avl_node *avl_insert(avl_node *root, int key)
{
    if (root == NULL) {
        avl_node *nn = (avl_node *)malloc(sizeof(avl_node));
        if (nn == NULL) return NULL;        /* FIX:检查 malloc 失败,原代码会空指针写入 */
        nn->key    = key;
        nn->height = 1;
        nn->left   = NULL;
        nn->right  = NULL;
        return nn;
    }

    if (key < root->key) {                  /* FIX:改成单 if + else if,
                                               原来是两个独立 if,逻辑虽对但多余比较 */
        root->left  = avl_insert(root->left, key);
        /* FIX:NULL 既表示分配失败也可能是"重复 key 未插入"的正常返回,
           不能再用 == NULL 直接短路(那样会丢掉祖先的 rebalance);
           只有整棵树原本为空(本次分配的就是新根)才需提前返回 */
        if (root->left == NULL && avl_height(root) == 0) return root; /* 分配失败,原树不变 */
    } else if (key > root->key) {
        root->right = avl_insert(root->right, key);
        if (root->right == NULL && avl_height(root) == 0) return root;
    }
    /* key 相等:不插入,直接返回原树(不泄漏) */

    return rebalance(root);
}

/* ================= 删除 ================= */

avl_node *avl_delete(avl_node *root, int key)
{
    if (root == NULL) return root;

    if (key < root->key) {
        root->left = avl_delete(root->left, key);
    } else if (key > root->key) {
        root->right = avl_delete(root->right, key);
    } else {
        /* 找到待删节点 */
        if (root->left == NULL || root->right == NULL) {
            avl_node *temp = root->left ? root->left : root->right;
            avl_node *dead = root;      /* FIX:原来单孩子分支直接 *root = *temp,
                                           会把 temp 的孩子拷进 root 后又 free(temp),
                                           造成"复制后再释放"的双重释放/悬空指针;
                                           统一改为摘除待删节点并释放它本身 */
            root = temp;                /* 单孩子:孩子顶上来;叶子:NULL */
            free(dead);
        } else {                       /* 双孩子:用右子树最小值顶替 */
            avl_node *temp = avl_min(root->right);
            root->key = temp->key;
            root->right = avl_delete(root->right, temp->key);
        }
    }

    if (root == NULL) return root;
    return rebalance(root);
}

/* ================= 销毁/遍历/校验 ================= */

void avl_destroy(avl_node *root)
{
    if (root == NULL) return;
    avl_destroy(root->left);
    avl_destroy(root->right);
    free(root);
}

void avl_inorder(const avl_node *root)
{
    if (root == NULL) return;
    avl_inorder(root->left);
    printf("%d ", root->key);
    avl_inorder(root->right);
}

/* 校验:1) 是 BST  2) 每个节点 |bf| <= 1  3) height 字段正确
 * lo/hi 是开区间边界,表示当前子树允许的 key 范围。 */
static int check_rec(const avl_node *n, long lo, long hi)
{
    if (n == NULL) return 1;

    if ((long)n->key <= lo || (long)n->key >= hi) return 0;   /* BST 性质 */

    if (avl_balance(n) > 1 || avl_balance(n) < -1) return 0;  /* AVL 性质 */

    int want = imax(avl_height(n->left), avl_height(n->right)) + 1;
    if (n->height != want) return 0;                          /* height 缓存正确 */

    if (!check_rec(n->left,  lo, n->key)) return 0;
    if (!check_rec(n->right, n->key, hi)) return 0;
    return 1;
}

int avl_check(const avl_node *root)
{
    return check_rec(root, LONG_MIN, LONG_MAX);
}