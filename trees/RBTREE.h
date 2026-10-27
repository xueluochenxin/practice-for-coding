#ifndef _RBTREE_H_
#define _RBTREE_H_

#include <stdio.h>
#include <stdlib.h>

typedef unsigned char color_t;
#define RED   ((color_t)1)
#define BLACK ((color_t)2)

#ifndef KEY_TYPE
typedef int KEY_TYPE;
#endif

/* 让节点结构可以嵌入任意业务结构体(侵入式设计) */
#define RBTREE_ENTRY(name, type) \
struct name {                    \
    struct type *right;          \
    struct type *left;           \
    struct type *parent;         \
    color_t      color;          \
}

/* 红黑树节点结构 */
typedef struct rbtree_node {
    KEY_TYPE key;
    void    *value;   /* 业务数据指针 */
    RBTREE_ENTRY(, rbtree_node) node;
} rbtree_node;

/* 红黑树根节点与哨兵结构 */
typedef struct rbtree {
    struct rbtree_node *root;
    struct rbtree_node *nil; /* 哨兵:颜色为黑,left/right 指向自身,parent 为 NULL */
} rbtree;

/* ==================== 公有 API 接口 ==================== */

/* 创建与销毁 */
rbtree *RBTREE_create(void);
void    RBTREE_destroy(rbtree *T);   /* FIX:原来声明 destroy、实现 distory,拼写不一致 */

/* 查找:返回找到的节点指针,没找到返回 NULL */
rbtree_node *RBTREE_search(rbtree *T, KEY_TYPE key);

/* 插入:
 * 注意:传进来的 z 必须是 malloc 出来的,key 和 value 必须在外部填好。
 * 返回值:0 成功,-1 表示 key 已存在(此时节点未插入,外部需自行释放 z)。 */
int RBTREE_insert(rbtree *T, rbtree_node *z);

/* 删除:
 * 注意:此函数只会 free 节点本身,不会 free(z->value)。
 * 返回值:0 成功,-1 表示 key 不存在。 */
int RBTREE_delete(rbtree *T, KEY_TYPE key);

/* 校验与打印(用于测试) */
int  RBTREE_check(rbtree *T);        /* 返回 1 合法,0 非法 */
void RBTREE_inorder(rbtree *T, rbtree_node *node); /* 中序遍历打印 */

#endif /* _RBTREE_H_ */