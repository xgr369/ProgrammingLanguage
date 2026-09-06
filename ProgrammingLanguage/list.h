#ifndef LIST_H
#define LIST_H

#include <stdlib.h>

typedef struct {
	char *data;
	size_t elemSize;
	size_t length;
	size_t size;
} LangM_List;

int langM_list_init(LangM_List *pl, size_t elemSize, size_t size);
int langM_list_clear(LangM_List *pl, size_t size);
int langM_list_free(LangM_List *pl);
void *langM_list_at(LangM_List *pl, size_t index);
int langM_list_get(LangM_List *pl, size_t index, void *dst);
int langM_list_pop(LangM_List *pl, void *dst);
int langM_list_popn(LangM_List *pl, size_t n);
int langM_list_push(LangM_List *pl, const void *src);
int langM_list_removen(LangM_List *pl, size_t index, size_t n);
int langM_list_set(LangM_List *pl, size_t index, void *src);

#endif // LIST_H