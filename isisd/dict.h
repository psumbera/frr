#ifndef _ISISD_DICT_H
#define _ISISD_DICT_H

#include <stdlib.h>

typedef struct dnode {
	const void *key;
	void *data;
	struct dnode *next;
} dnode_t;

typedef int (*dict_cmp_fn)(const void *key1, const void *key2);

typedef struct dict {
	dict_cmp_fn cmp;
	dnode_t *head;
} dict_t;

static inline dict_t *dict_create(size_t max, dict_cmp_fn cmp)
{
	(void)max;
	dict_t *dict = calloc(1, sizeof(*dict));
	if (dict)
		dict->cmp = cmp;
	return dict;
}

static inline dnode_t *dict_lookup(dict_t *dict, const void *key)
{
	for (dnode_t *node = dict ? dict->head : NULL; node; node = node->next)
		if (dict->cmp(key, node->key) == 0)
			return node;
	return NULL;
}

static inline dnode_t *dict_alloc_insert(dict_t *dict, const void *key, void *data)
{
	dnode_t *node = calloc(1, sizeof(*node));
	if (!node)
		return NULL;
	node->key = key;
	node->data = data;
	node->next = dict->head;
	dict->head = node;
	return node;
}

static inline void dict_delete_free(dict_t *dict, dnode_t *dnode)
{
	dnode_t **iter;

	if (!dict || !dnode)
		return;
	for (iter = &dict->head; *iter; iter = &(*iter)->next) {
		if (*iter == dnode) {
			*iter = dnode->next;
			free(dnode);
			return;
		}
	}
}

static inline dnode_t *dict_first(dict_t *dict)
{
	return dict ? dict->head : NULL;
}

static inline dnode_t *dict_next(dict_t *dict, dnode_t *dnode)
{
	(void)dict;
	return dnode ? dnode->next : NULL;
}

static inline void dict_free_nodes(dict_t *dict)
{
	(void)dict;
}

static inline void dict_destroy(dict_t *dict)
{
	free(dict);
}

static inline void *dnode_get(dnode_t *dnode)
{
	return dnode ? dnode->data : NULL;
}

#endif
