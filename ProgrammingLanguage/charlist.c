#include "charlist.h"
#include <string.h>

static int charlist_resize(LangM_CharList *pcl, size_t addLen) {
	while (pcl->length + addLen > pcl->size)
		pcl->size = pcl->size ? pcl->size * 2 : 1;
	void* ptr = realloc(pcl->data, pcl->size);
	if (!ptr)
		return 1;
	pcl->data = ptr;
	return 0;
}

int langM_charlist_init(LangM_CharList *pcl, size_t size) {
	if (!pcl)
		return 1;
	pcl->data = malloc(size);
	if (!pcl->data)
		return 1;
	pcl->length = 0;
	pcl->size = size;
	return 0;
}

int langM_charlist_free(LangM_CharList *pcl) {
	if (!pcl || !pcl->data)
		return 1;
	free(pcl->data);
	pcl->data = NULL;
	return 0;
}

int langM_charlist_get(LangM_CharList *pcl, size_t index, char *dst) {
	if (!pcl || !dst || index < 0 || index >= pcl->length)
		return 1;
	memcpy(dst, pcl->data + index, 1);
	return 0;
}

int langM_charlist_pop(LangM_CharList *pcl, char *dst) {
	if (!pcl || !dst || pcl->length == 0)
		return 1;
	memcpy(dst, pcl->data + pcl->length - 1, 1);
	pcl->length--;
	return 0;
}

int langM_charlist_push(LangM_CharList *pcl, char value) {
	if (!pcl)
		return 1;
	if (pcl->length >= pcl->size)
		if (charlist_resize(pcl, 1))
			return 1;
	*(pcl->data + pcl->length) = value;
	pcl->length++;
	return 0;
}

int langM_charlist_pusharray(LangM_CharList *pcl, const char *src, size_t len) {
	if (!pcl || !src)
		return 1;
	if (pcl->length + len > pcl->size)
		if (charlist_resize(pcl, len))
			return 1;
	memcpy(pcl->data + pcl->length, src, len);
	pcl->length += len;
	return 0;
}

int langM_charlist_setarray(LangM_CharList *pcl, size_t index, const char *src, size_t len) {
	if (!pcl || !src)
		return 1;
	if (index + len > pcl->size)
		if (charlist_resize(pcl, len))
			return 1;
	memcpy(pcl->data + index, src, len);
	return 0;
}