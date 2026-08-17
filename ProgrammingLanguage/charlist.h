#ifndef CHARLIST_H
#define CHARLIST_H

#include <stdlib.h>

typedef struct {
	char* data;
	size_t length;
	size_t size;
} LangM_CharList;

int langM_charlist_init(LangM_CharList *pcv, size_t size);
int langM_charlist_free(LangM_CharList *pcv);
int langM_charlist_get(LangM_CharList *pcv, size_t index, char *dst);
int langM_charlist_pop(LangM_CharList *pcv, char *dst);
int langM_charlist_push(LangM_CharList *pcv, char value);
int langM_charlist_pusharray(LangM_CharList *pcv, const char *src, size_t len);
int langM_charlist_setarray(LangM_CharList *pcv, size_t index, const char *src, size_t len);

#endif // CHARLIST_H