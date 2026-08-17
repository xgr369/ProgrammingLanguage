#include "hash.h"
#include <string.h>

#define STRING_HASH_TABLE_SEED 5381

int langM_hash_hash(const char *key) {
	unsigned long hash = STRING_HASH_TABLE_SEED;
	int c;
	while ((c = (unsigned char)*key++)) {
		hash = ((hash << 5) + hash) + c;
	}
	return (int)hash;
}

int langM_hashnode_free(LangM_HashNode *phn) {
	if (phn->next)
		langM_hashnode_free(phn->next);
	free(phn->key);
	free(phn);
	return 0;
}

void *langM_hashnode_at(LangM_HashNode *phn, const char *key) {
	if (!phn)
		return NULL;
	if (strcmp(phn->key, key) == 0)
		return phn->value;
	return langM_hashnode_at(phn->next, key);
}

int langM_hashnode_containskey(LangM_HashNode *phn, const char *key) {
	if (!phn)
		return 1;
	if (strcmp(phn->key, key) == 0)
		return 0;
	return langM_hashnode_containskey(phn->next, key);
}

LangM_HashNode *langM_hashnode_get(LangM_HashNode *phn, const char *key) {
	if (!phn)
		return NULL;
	if (strcmp(phn->key, key) == 0)
		return phn;
	return langM_hashnode_get(phn->next, key);
}

LangM_HashNode *langM_hashnode_new(const char *key, const void *src, size_t elemSize) {
	size_t entrySize = sizeof(LangM_HashNode) + elemSize;
	LangM_HashNode *phn = malloc(entrySize);
	phn->key = _strdup(key);
	phn->next = NULL;
	memcpy(phn->value, src, elemSize);
	return phn;
}

int langM_hashnode_put(LangM_HashNode *phn, const char *key, const void *src, size_t elemSize) {
	if (strcmp(phn->key, key) == 0) {
		memcpy(phn->value, src, elemSize);
		return 0;
	}
	if (!phn->next) {
		LangM_HashNode *pshteNext = langM_hashnode_new(key, src, elemSize);
		if (!pshteNext)
			return 1;
		phn->next = pshteNext;
		return 0;
	}
	return langM_hashnode_put(phn->next, key, src, elemSize);
}

int langM_hash_init(LangM_Hash *ph, size_t elemSize, size_t size) {
	if (!ph || elemSize == 0)
		return 1;
	size_t entrySize = sizeof(LangM_HashNode) + elemSize;
	ph->data = malloc(size * entrySize);
	if (!ph->data)
		return 1;
	for (size_t i = 0; i < size; i++)
		ph->data[i] = NULL;
	ph->size = size;
	ph->elemSize = elemSize;
	return 0;
}
int langM_hash_clear(LangM_Hash *ph, size_t size) {
	if (!ph)
		return 1;
	for (size_t i = 0; i < ph->size; i++) {
		LangM_HashNode *phn = ph->data[i];
		if (!phn)
			continue;
		langM_hashnode_free(phn);
	}
	size_t entrySize = sizeof(LangM_HashNode) + ph->elemSize;
	void *ptr = realloc(ph->data, size * entrySize);
	if (!ptr)
		return 1;
	ph->data = ptr;
	for (size_t i = 0; i < size; i++)
		ph->data[i] = NULL;
	return 0;
}
int langM_hash_free(LangM_Hash *ph) {
	if (!ph)
		return 1;
	for (size_t i = 0; i < ph->size; i++) {
		LangM_HashNode *phn = ph->data[i];
		if (!phn)
			continue;
		langM_hashnode_free(phn);
	}
	free(ph->data);
	return 0;
}
void *langM_hash_at(LangM_Hash *ph, const char *key) {
	if (!ph || !key)
		return NULL;
	size_t idx = langM_hash_hash(key) % ph->size;
	LangM_HashNode *phn = langM_hashnode_at(ph->data[idx], key);
	return phn;
}
int langM_hash_containskey(LangM_Hash *ph, const char *key) {
	if (!ph || !key)
		return 1;
	size_t idx = langM_hash_hash(key) % ph->size;
	LangM_HashNode *phn = langM_hashnode_get(ph->data[idx], key);
	return langM_hashnode_containskey(phn, key);
}
int langM_hash_get(LangM_Hash *ph, const char *key, void *dst) {
	if (!ph || !key || !dst)
		return 1;
	size_t idx = langM_hash_hash(key) % ph->size;
	LangM_HashNode *phn = langM_hashnode_get(ph->data[idx], key);
	if (!phn)
		return 1;
	memcpy(dst, phn->value, ph->elemSize);
	return 0;
}
int langM_hash_put(LangM_Hash *ph, const char *key, const void *src) {
	if (!ph || !key || !src)
		return 1;
	size_t idx = langM_hash_hash(key) % ph->size;
	LangM_HashNode *phn = ph->data[idx];
	if (!phn) {
		phn = langM_hashnode_new(key, src, ph->elemSize);
		if (!phn)
			return 1;
		ph->data[idx] = phn;
		return 0;
	}
	return langM_hashnode_put(phn, key, src, ph->elemSize);

}
int langM_hash_remove(LangM_Hash *ph, const char *key, void *dst) {
	if (!ph || !key)
		return 1;
	size_t idx = langM_hash_hash(key) % ph->size;
	LangM_HashNode *phn = langM_hashnode_get(ph->data[idx], key);
	if (!phn)
		return 1;
	if (dst)
		memcpy(dst, phn->value, ph->elemSize);
	// free phste
	// change previous.next to NULL
}