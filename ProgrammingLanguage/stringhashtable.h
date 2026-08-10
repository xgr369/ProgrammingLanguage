#ifndef STRING_HASH_TABLE_H
#define STRING_HASH_TABLE_H

#include <stdlib.h>

struct StringHashTableEntry {
	char *key;
	struct StringHashTableEntry *next;
	char value[];
};
typedef struct StringHashTableEntry StringHashTableEntry;

typedef struct {
	StringHashTableEntry **data;
	size_t elemSize;
	size_t size;
} StringHashTable;

int stringhashtable_init(StringHashTable *psht, size_t elemSize, size_t size);
int stringhashtable_free(StringHashTable *psht);
int stringhashtable_containskey(StringHashTable *psht, const char *key);
int stringhashtable_get(StringHashTable *psht, const char *key, void *dst);
int stringhashtable_put(StringHashTable *psht, const char *key, const void *src);
//int stringhashtable_remove(StringHashTable *psht, const char *key, void *dst);

#endif // STRING_HASH_TABLE_H