#ifndef HASHMAP_H
#define HASHMAP_H

/* ============================================================================
 * hashmap.h —— 开放寻址(线性探测)哈希表库
 *
 * 设计要点:
 *   - 键:   int(可按下方方式改为其它整型)
 *   - 值:   void*,由调用方分配和释放,本库只保存指针
 *   - 存储: 数组 + 线性探测,负载因子上限 0.7 自动扩容(容量翻倍)
 *   - 删除: 惰性删除(DELETED 墓碑标记),rehash 时统一回收空位
 *   - 线程不安全,单线程使用
 *
 * 用法示例:
 *   hashmap *m = hashmap_create(0);          // 0 = 默认容量,也可指定
 *   int *v = malloc(sizeof *v); *v = 42;
 *   hashmap_put(m, 100, v);                 // v 的所有权转给调用方管理
 *   int *got = hashmap_get(m, 100);          // 找到返回值,否则 NULL
 *   if (hashmap_remove(m, 100)) free(got);   // remove 返回 1 表示删除成功
 *   hashmap_destroy(m, NULL);               // 第二参数为每个 value 的释放函数
 *
 * 对外提供的接口(全部以 hashmap_ 前缀命名,不会与其它库冲突):
 *   创建/销毁  : hashmap_create, hashmap_destroy, hashmap_clear
 *   增/删/改   : hashmap_put, hashmap_remove
 *   查/读      : hashmap_get, hashmap_contains, hashmap_size, hashmap_empty
 *   遍历       : hashmap_foreach, hashmap_keys
 *   扩展       : hashmap_reserve
 *   错误处理   : hashmap_last_error
 * ==========================================================================*/

#include <stddef.h>

/* ---------- 可选:在 #include "hashmap.h" 之前定义 KEY_TYPE 覆盖键类型 ---------- */
#ifndef KEY_TYPE
typedef int KEY_TYPE;
#endif

/* 表状态:供 hashmap_last_error() 返回 */
#define HASHMAP_OK            0   /* 上次操作成功 */
#define HASHMAP_ERR_OOM       1   /* 内存分配失败,操作未生效 */
#define HASHMAP_ERR_FULL      2   /* 表已满且无法扩容(仅 hashmap_reserve 触发) */
#define HASHMAP_ERR_NOTFOUND  3   /* 键不存在(remove/put 读取用) */

/* 遍历回调:对每个 (key, value) 调用一次。
 * 返回 0 表示继续遍历,返回非 0 提前终止。 */
typedef int (*hashmap_iter_fn)(KEY_TYPE key, void *value, void *userdata);

/* value 释放回调:destroy/clear 时对每个 value 调用一次(可为 NULL) */
typedef void (*hashmap_val_free_fn)(void *value);

/* 对外暴露的哈希表结构体(字段视为内部实现,请勿直接修改) */
typedef struct hashmap hashmap;

/* ======================= 对外接口 ======================= */

/* --- 创建 / 销毁 --- */

/* 创建哈希表。initial_cap 传 0 使用默认容量(16);超过 0 则向上取 2 的幂。
 * 成功返回表指针,内存不足返回 NULL。 */
hashmap *hashmap_create(size_t initial_cap);

/* 销毁整张表并释放所有桶。
 * free_value 非 NULL 时,会依次对每个仍存在的 value 调用它(交给你的业务数据释放器)。
 * 传 NULL 表示本库不碰 value,由调用方自行释放。
 * 销毁后指针不可再使用。 */
void hashmap_destroy(hashmap *m, hashmap_val_free_fn free_value);

/* 清空表内所有键值对(保留已分配的容量,不缩小)。
 * free_value 语义同 destroy。 */
void hashmap_clear(hashmap *m, hashmap_val_free_fn free_value);

/* --- 增 / 删 / 改 --- */

/* 插入或更新 key。value 直接存储指针,所有权仍归调用方。
 * 返回 1 表示插入成功;0 表示更新了已有键;
 * -1 表示内存分配失败(见 hashmap_last_error)。 */
int hashmap_put(hashmap *m, KEY_TYPE key, void *value);

/* 删除 key。成功返回 1;key 不存在返回 0。 */
int hashmap_remove(hashmap *m, KEY_TYPE key);

/* --- 查 / 读 --- */

/* 取 key 对应的值;不存在返回 NULL。
 * 注意:value 本身为 NULL 时无法与"不存在"区分,用 hashmap_contains 判断。 */
void *hashmap_get(const hashmap *m, KEY_TYPE key);

/* 判断 key 是否存在,返回 1 / 0。 */
int hashmap_contains(const hashmap *m, KEY_TYPE key);

/* 返回键值对数量。 */
size_t hashmap_size(const hashmap *m);

/* 表为空返回 1,否则 0。 */
int hashmap_empty(const hashmap *m);

/* --- 遍历 --- */

/* 依次对每个 (key, value) 调用 fn;fn 返回非 0 时提前结束。
 * 遍历期间禁止 put/remove(未定义行为)。fn 返回累计的非零值,全遍历完返回 0。 */
int hashmap_foreach(const hashmap *m, hashmap_iter_fn fn, void *userdata);

/* 把所有 key 拷贝到 out_keys 数组(容量至少 hashmap_size(m)),返回实际写入个数。
 * 用于需要索引访问的场景;out_keys 由调用方分配。 */
size_t hashmap_keys(const hashmap *m, KEY_TYPE *out_keys);

/* --- 扩展 --- */

/* 预留至少能容纳 n 个元素的容量(不足才扩容,幂次向上取)。
 * 成功返回 0;内存不足返回 -1(见 hashmap_last_error)。 */
int hashmap_reserve(hashmap *m, size_t n);

/* --- 错误处理 --- */

/* 返回上一次可能失败的操作的状态码(HASHMAP_OK / OOM / FULL / NOTFOUND)。 */
int hashmap_last_error(const hashmap *m);

#endif /* HASHMAP_H */