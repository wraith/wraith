#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_

#define HASH_TABLE_STRINGS	BIT0
#define HASH_TABLE_INTS		BIT1
#define HASH_TABLE_MIXED	BIT2
#define HASH_TABLE_NORESIZE	BIT3

/* Turns a key into an unsigned int. */
typedef unsigned int (*hash_table_hash_alg)(const void *key);

/* Returns -1, 0, or 1 if left is <, =, or > than right. */
typedef int (*hash_table_cmp_alg)(const void *left, const void *right);

typedef int (*hash_table_node_func)(const void *key, void *data, void *param);

typedef struct hash_table_entry_b {
	struct hash_table_entry_b *next;
	const void *key;
	void *data;
	unsigned int hash;
} hash_table_entry_t;

typedef struct {
	int len;
	hash_table_entry_t *head;
} hash_table_row_t;

typedef struct hash_table_b {
	int flags;
	int max_rows;
	int cells_in_use;
	hash_table_hash_alg hash;
	hash_table_cmp_alg cmp;
	hash_table_row_t *rows;
} hash_table_t;

hash_table_t *hash_table_create(hash_table_hash_alg alg, hash_table_cmp_alg cmp, int nrows, int flags);
int hash_table_delete(hash_table_t *ht);
int hash_table_check_resize(hash_table_t *ht);
int hash_table_resize(hash_table_t *ht, int nrows);
int hash_table_insert(hash_table_t *ht, const void *key, void *data);
int hash_table_replace(hash_table_t *ht, const void *key, void *data);
int hash_table_find(hash_table_t *ht, const void *key, void *dataptr);
int hash_table_remove(hash_table_t *ht, const void *key, void *dataptr);
int hash_table_walk(hash_table_t *ht, hash_table_node_func callback, void *param);

#endif /* !_HASH_TABLE_H_ */
