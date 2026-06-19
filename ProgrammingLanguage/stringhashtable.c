#include "stringhashtable.h"
#include <string.h>

#define STRING_HASH_TABLE_SEED 5381

int stringhashtable_hash(const char *key) {
	unsigned long hash = STRING_HASH_TABLE_SEED;
	int c;
	while ((c = (unsigned char)*key++)) {
		hash = ((hash << 5) + hash) + c;
	}
	return (int)hash;
}

int stringhashtableentry_free(StringHashTableEntry *pshte) {
	if (pshte->next)
		stringhashtableentry_free(pshte->next);
	free(pshte->key);
	free(pshte);
	return 0;
}

int stringhashtableentry_containskey(StringHashTableEntry *pshte, const char *key) {
	if (!pshte)
		return 1;
	if (strcmp(pshte->key, key) == 0)
		return 0;
	return stringhashtableentry_containskey(pshte->next, key);
}

StringHashTableEntry *stringhashtableentry_get(StringHashTableEntry *pshte, const char *key) {
	if (!pshte)
		return NULL;
	if (strcmp(pshte->key, key) == 0)
		return pshte;
	return stringhashtableentry_get(pshte->next, key);
}

StringHashTableEntry *stringhashtableentry_new(const char *key, const void *src, size_t elemSize) {
	size_t entrySize = sizeof(StringHashTableEntry) + elemSize;
	StringHashTableEntry *pshte = malloc(entrySize);
	pshte->key = _strdup(key);
	pshte->next = NULL;
	memcpy(pshte->value, src, elemSize);
	return pshte;
}

int stringhashtableentry_put(StringHashTableEntry *pshte, const char *key, const void *src, size_t elemSize) {
	if (strcmp(pshte->key, key) == 0) {
		memcpy(pshte->value, src, elemSize);
		return 0;
	}
	if (!pshte->next) {
		StringHashTableEntry *pshteNext = stringhashtableentry_new(key, src, elemSize);
		if (!pshteNext)
			return 1;
		pshte->next = pshteNext;
		return 0;
	}
	return stringhashtableentry_put(pshte->next, key, src, elemSize);
}

int stringhashtable_new(StringHashTable *psht, size_t elemSize, size_t size) {
	if (!psht || elemSize == 0)
		return 1;
	size_t entrySize = sizeof(StringHashTableEntry) + elemSize;
	psht->data = malloc(size * entrySize);
	if (!psht->data)
		return 1;
	psht->length = 0;
	psht->size = size;
	psht->elemSize = elemSize;
	for (size_t i = 0; i < psht->size; i++) {
		psht->data[i] = NULL;
	}
	return 0;
}
int stringhashtable_free(StringHashTable *psht) {
	if (!psht)
		return 1;
	for (size_t i = 0; i < psht->length; i++) {
		StringHashTableEntry *pshte = psht->data[i];
		if (!pshte)
			continue;
		stringhashtableentry_free(pshte);
	}
	free(psht->data);
	return 0;
}
int stringhashtable_containskey(StringHashTable *psht, const char *key) {
	size_t idx = stringhashtable_hash(key) % psht->size;
	StringHashTableEntry *pshte = stringhashtableentry_get(psht->data[idx], key);
	return stringhashtableentry_containskey(pshte, key);
}
int stringhashtable_get(StringHashTable *psht, const char *key, void *dst) {
	if (!psht || !key || !dst)
		return 1;
	size_t idx = stringhashtable_hash(key) % psht->size;
	StringHashTableEntry *pshte = stringhashtableentry_get(psht->data[idx], key);
	if (!pshte)
		return 1;
	memcpy(dst, pshte->value, psht->elemSize);
	return 0;
}
int stringhashtable_put(StringHashTable *psht, const char *key, const void *src) {
	if (!psht || !key || !src)
		return 1;
	size_t idx = stringhashtable_hash(key) % psht->size;
	StringHashTableEntry *pshte = psht->data[idx];
	if (!pshte) {
		pshte = stringhashtableentry_new(key, src, psht->elemSize);
		if (!pshte)
			return 1;
		psht->data[idx] = pshte;
		return 0;
	}
	return stringhashtableentry_put(pshte, key, src, psht->elemSize);
	
}
int stringhashtable_remove(StringHashTable *psht, const char *key, void *dst) {
	if (!psht || !key)
		return 1;
	size_t idx = stringhashtable_hash(key) % psht->size;
	StringHashTableEntry *pshte = stringhashtableentry_get(psht->data[idx], key);
	if (!pshte)
		return 1;
	if (dst)
		memcpy(dst, pshte->value, psht->elemSize);
	// free phste
	// change previous.next to NULL
}