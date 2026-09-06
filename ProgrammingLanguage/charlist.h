#ifndef CHARLIST_H
#define CHARLIST_H

#include <stdlib.h>

typedef struct {
	char* data;
	size_t length;
	size_t size;
} LangM_CharList;

int langM_charlist_init(LangM_CharList *pcl, size_t size);
int langM_charlist_free(LangM_CharList *pcl);
int langM_charlist_get(LangM_CharList *pcl, size_t index, char *dst);
int langM_charlist_pop(LangM_CharList *pcl, char *dst);
int langM_charlist_push(LangM_CharList *pcl, char value);
int langM_charlist_pusharray(LangM_CharList *pcl, const char *src, size_t len);
int langM_charlist_setarray(LangM_CharList *pcl, size_t index, const char *src, size_t len);

#endif // CHARLIST_H