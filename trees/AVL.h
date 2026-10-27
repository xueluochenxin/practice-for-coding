#ifndef AVL_H
#define AVL_H

/* AVL 树 (自平衡二叉搜索库)
 * 叶子 height 记作 1,空节点 height 为 0。
 * 所有对外函数统一加 avl_ 前缀,避免与其它库命名冲突。
 */

typedef struct avl_node
{
    int key;
    int height;               /* 叶子 height 记作 1 */
    struct avl_node *left;
    struct avl_node *right;
} avl_node;

/* ---------- 查询/工具 ---------- */
int      avl_height(const avl_node *x);       /* 空节点返回 0 */
int      avl_balance(const avl_node *x);      /* 空节点返回 0 */
avl_node *avl_search(avl_node *root, int key);
avl_node *avl_min(avl_node *root);            /* 最小值节点,NULL 安全 */
avl_node *avl_max(avl_node *root);            /* 最大值节点,NULL 安全 */

/* ---------- 增删(返回新的子树根) ---------- */
/* 返回值可能是新的根,调用方必须用返回值覆盖原 root。
 * 插入重复 key 不会创建新节点,直接返回原树,不泄漏。
 * 内存分配失败时返回 NULL(原树不变)。
 */
avl_node *avl_insert(avl_node *root, int key);

/* 删除 key;key 不存在时原样返回 root。
 * 删除后自动旋转保持平衡。 */
avl_node *avl_delete(avl_node *root, int key);

/* 释放整棵树所有节点 */
void avl_destroy(avl_node *root);

/* 中序遍历(测试用):按 key 升序打印 */
void avl_inorder(const avl_node *root);

/* 校验 AVL 性质:返回 1 合法,0 非法(测试用) */
int avl_check(const avl_node *root);

#endif /* AVL_H */