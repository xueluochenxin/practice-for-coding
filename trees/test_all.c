/* 综合测试:AVL / RBTREE / hashmap / skiplist 四个库 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "AVL.h"
#include "RBTREE.h"
#include "hashmap.h"
#include "skiplist.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); failures++; } \
} while (0)

/* ---------------- AVL ---------------- */
static void test_avl(void)
{
    avl_node *root = NULL;
    int i;

    for (i = 0; i < 500; i++)
        root = avl_insert(root, (i * 37) % 500);   /* 乱序插入 0..499 */
    CHECK(avl_check(root) == 1, "AVL insert keeps invariants");
    CHECK(avl_height(root) <= 12, "AVL height is logarithmic");

    CHECK(avl_search(root, 123) != NULL, "AVL search hit");
    CHECK(avl_search(root, 10000) == NULL, "AVL search miss");

    for (i = 0; i < 500; i += 2)
        root = avl_delete(root, (i * 37) % 500);   /* 删掉一半 */
    CHECK(avl_check(root) == 1, "AVL delete keeps invariants");

    for (i = 0; i < 500; i += 2)
        root = avl_delete(root, (i * 37) % 500);   /* 重复删除应原样返回 */
    CHECK(avl_check(root) == 1, "AVL deleting missing key is no-op");

    int cnt = 0;
    {   /* 数一下剩余节点 */
        avl_node *stk[1024]; int sp = 0;
        if (root) stk[sp++] = root;
        while (sp) {
            avl_node *n = stk[--sp]; cnt++;
            if (n->left)  stk[sp++] = n->left;
            if (n->right) stk[sp++] = n->right;
        }
    }
    CHECK(cnt == 250, "AVL remaining count == 250");

    for (i = 1; i < 500; i += 2) root = avl_delete(root, (i * 37) % 500);
    CHECK(root == NULL, "AVL fully drained");

    /* 单孩子删除路径(原 *root=*temp 双重释放就在这里触发) */
    root = avl_insert(NULL, 50);
    root = avl_insert(root, 30);
    root = avl_insert(root, 40);
    root = avl_insert(root, 60);
    root = avl_delete(root, 50);          /* 双孩子 */
    CHECK(avl_check(root) == 1, "AVL delete root with two children");
    root = avl_delete(root, 60);          /* 单孩子/叶子 */
    CHECK(avl_check(root) == 1, "AVL delete single-child path");
    avl_destroy(root);

    printf("AVL tests done\n");
}

/* ---------------- RBTREE ---------------- */
static rbtree_node *make_node(int key)
{
    rbtree_node *n = (rbtree_node *)calloc(1, sizeof(rbtree_node));
    n->key = key;
    n->value = NULL;
    return n;
}

static void test_rbtree(void)
{
    rbtree *T = RBTREE_create();
    CHECK(T != NULL, "RBTREE create");

    int i;
    for (i = 0; i < 1000; i++) {
        int k = (i * 617) % 1000;
        rbtree_node *n = make_node(k);
        int r = RBTREE_insert(T, n);
        if (r != 0) { free(n); CHECK(0, "RBTREE insert should succeed"); }
    }
    CHECK(RBTREE_check(T) == 1, "RBTREE check after 1000 inserts");

    /* 重复插入应返回 -1 */
    {
        rbtree_node *dup = make_node(5);
        CHECK(RBTREE_insert(T, dup) == -1, "RBTREE duplicate insert returns -1");
        free(dup);
    }

    CHECK(RBTREE_search(T, 617) != NULL, "RBTREE search hit");
    CHECK(RBTREE_search(T, 123456) == NULL, "RBTREE search miss");

    /* 大量删除,反复校验(覆盖双孩子/红黑修复路径) */
    for (i = 0; i < 1000; i++) {
        int k = (i * 617) % 1000;
        int r = RBTREE_delete(T, k);
        if (r != 0) { CHECK(0, "RBTREE delete should succeed"); break; }
        if (i % 97 == 0 && RBTREE_check(T) != 1) {
            printf("FAIL: RBTREE check broken after deleting %d keys\n", i + 1);
            failures++;
            break;
        }
    }
    CHECK(RBTREE_check(T) == 1, "RBTREE check after draining");
    CHECK(RBTREE_search(T, 0) == NULL, "RBTREE empty after drain");
    CHECK(RBTREE_delete(T, 1) == -1, "RBTREE delete missing key == -1");

    RBTREE_destroy(T);
    printf("RBTREE tests done\n");
}

/* ---------------- hashmap ---------------- */
static int count_cb(KEY_TYPE k, void *v, void *ud) { (void)k; (void)v; (*(int *)ud)++; return 0; }

static void test_hashmap(void)
{
    hashmap *m = hashmap_create(0);
    CHECK(m != NULL, "hashmap create");
    CHECK(hashmap_size(m) == 0, "hashmap starts empty");

    int i;
    for (i = 0; i < 3000; i++) {
        int *v = (int *)malloc(sizeof(int));
        *v = i;
        CHECK(hashmap_put(m, i, v) == 1, "hashmap put new");
    }
    CHECK(hashmap_size(m) == 3000, "hashmap size 3000");

    int *got = (int *)hashmap_get(m, 1234);
    CHECK(got && *got == 1234, "hashmap get");

    /* 更新已有键 */
    int nv = 999;
    CHECK(hashmap_put(m, 1234, &nv) == 0, "hashmap update returns 0");

    /* 删除一半,再查 */
    for (i = 0; i < 3000; i += 2) CHECK(hashmap_remove(m, i) == 1, "hashmap remove");
    CHECK(hashmap_size(m) == 1500, "hashmap size after removals");
    CHECK(hashmap_get(m, 2000) == NULL, "hashmap removed key gone");
    CHECK(hashmap_contains(m, 2001) == 1, "hashmap kept key present");

    int cnt = 0;
    CHECK(hashmap_foreach(m, count_cb, &cnt) == 0 && cnt == 1500, "hashmap foreach count");

    /* clear 后 used 必须归零:再插满同样规模不应盲目扩容、大小正确 */
    hashmap_clear(m, free);
    CHECK(hashmap_size(m) == 0, "hashmap clear size");
    CHECK(hashmap_empty(m) == 1, "hashmap empty after clear");
    for (i = 0; i < 3000; i++) {
        int *v = (int *)malloc(sizeof(int)); *v = i;
        CHECK(hashmap_put(m, i, v) == 1, "hashmap re-put after clear");
    }
    CHECK(hashmap_size(m) == 3000, "hashmap size after clear+reput");

    hashmap_destroy(m, free);
    printf("hashmap tests done\n");
}

/* ---------------- skiplist ---------------- */
static void test_skiplist(void)
{
    skiplist *s = skiplist_create(0);
    CHECK(s != NULL, "skiplist create");

    int i;
    for (i = 0; i < 2000; i++) {
        int *v = (int *)malloc(sizeof(int)); *v = i;
        int r = skiplist_insert(s, (i * 7919) % 2000, v);
        CHECK(r == 1, "skiplist insert new");
    }
    CHECK(skiplist_size(s) == 2000, "skiplist size");
    CHECK(skiplist_last_error(s) == SKIPLIST_OK, "skiplist last_error OK");

    int *g = (int *)skiplist_get(s, 123);
    CHECK(g && *g == 123, "skiplist get");
    CHECK(skiplist_get(s, 999999) == NULL, "skiplist get miss");
    CHECK(skiplist_last_error(s) == SKIPLIST_ERR_NOTFOUND, "skiplist notfound error code");

    /* 有序遍历 */
    int prev = -1, ordered = 1;
    for (skiplist_node *n = skiplist_first(s); n; n = skiplist_next(s, n)) {
        if (n->key <= prev) ordered = 0;
        prev = n->key;
    }
    CHECK(ordered == 1, "skiplist in-order traversal");

    /* 区间扫描 */
    int cnt = 0;
    skiplist_range(s, 100, 200, count_cb, &cnt);
    CHECK(cnt == 101, "skiplist range [100,200] count");

    /* 删除一半 */
    for (i = 0; i < 2000; i += 2) CHECK(skiplist_erase(s, (i * 7919) % 2000) == 1, "skiplist erase");
    CHECK(skiplist_size(s) == 1000, "skiplist size after erase");
    CHECK(skiplist_erase(s, 123456) == 0, "skiplist erase missing");

    skiplist_clear(s, free);
    CHECK(skiplist_empty(s) == 1, "skiplist empty after clear");

    skiplist_destroy(s, NULL);
    printf("skiplist tests done\n");
}

int main(void)
{
    test_avl();
    test_rbtree();
    test_hashmap();
    test_skiplist();

    if (failures == 0) printf("\nALL TESTS PASSED\n");
    else               printf("\n%d FAILURES\n", failures);
    return failures ? 1 : 0;
}
