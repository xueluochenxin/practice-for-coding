/* 最小可调用示例:验证每个库都能独立 include 并跑通 */
#include <stdio.h>
#include <stdlib.h>
#include "AVL.h"
#include "RBTREE.h"
#include "hashmap.h"
#include "skiplist.h"

int main(void)
{
    avl_node *t = avl_insert(NULL, 42);
    printf("AVL search 42 -> %s\n", avl_search(t, 42) ? "found" : "no");
    avl_destroy(t);

    rbtree *T = RBTREE_create();
    rbtree_node *n = (rbtree_node *)calloc(1, sizeof *n);
    n->key = 7;
    RBTREE_insert(T, n);
    printf("RB search 7 -> %s\n", RBTREE_search(T, 7) ? "found" : "no");
    RBTREE_destroy(T);

    hashmap *m = hashmap_create(0);
    int *v = malloc(sizeof *v); *v = 1;
    hashmap_put(m, 5, v);
    printf("hashmap get 5 -> %d\n", *(int *)hashmap_get(m, 5));
    hashmap_destroy(m, free);

    skiplist *s = skiplist_create(0);
    skiplist_insert(s, 9, v);
    printf("skiplist get 9 -> %d\n", *(int *)skiplist_get(s, 9));
    skiplist_destroy(s, free);

    printf("example OK\n");
    return 0;
}
