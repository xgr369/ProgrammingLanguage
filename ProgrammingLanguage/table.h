#ifndef STRING_HASH_TABLE_H
#define STRING_HASH_TABLE_H

#include <stdlib.h>

struct LangM_TableNode {
	char *key;
	struct LangM_TableNode *next;
	char value[];
};
typedef struct LangM_TableNode LangM_TableNode;

typedef struct {
	LangM_TableNode **data;
	size_t elemSize;
	size_t size;
	size_t count;
} LangM_Table;

int langM_table_init(LangM_Table *pt, size_t elemSize, size_t size);
int langM_table_clear(LangM_Table *pt, size_t size);
int langM_table_free(LangM_Table *pt);
void *langM_table_at(LangM_Table *pt, const char *key);
int langM_table_containskey(LangM_Table *pt, const char *key);
int langM_table_get(LangM_Table *pt, const char *key, void *dst);
int langM_table_put(LangM_Table *pt, const char *key, const void *src);
int langM_table_remove(LangM_Table *pt, const char *key, void *dst);

#endif // STRING_HASH_TABLE_H