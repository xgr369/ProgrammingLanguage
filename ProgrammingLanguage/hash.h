#ifndef STRING_HASH_TABLE_H
#define STRING_HASH_TABLE_H

#include <stdlib.h>

struct LangM_HashNode {
	char *key;
	struct LangM_HashNode *next;
	char value[];
};
typedef struct LangM_HashNode LangM_HashNode;

typedef struct {
	LangM_HashNode **data;
	size_t elemSize;
	size_t size;
} LangM_Hash;

int langM_hash_init(LangM_Hash *psht, size_t elemSize, size_t size);
int langM_hash_clear(LangM_Hash *psht, size_t size);
int langM_hash_free(LangM_Hash *psht);
void *langM_hash_at(LangM_Hash *psht, const char *key);
int langM_hash_containskey(LangM_Hash *psht, const char *key);
int langM_hash_get(LangM_Hash *psht, const char *key, void *dst);
int langM_hash_put(LangM_Hash *psht, const char *key, const void *src);
//int langM_hash_remove(LangM_Hash *psht, const char *key, void *dst);

#endif // STRING_HASH_TABLE_H