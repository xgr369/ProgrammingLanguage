#include "charvector.h"
#include <string.h>

int _charvector_resize(CharVector *pcv, size_t addLen) {
	while (pcv->length + addLen > pcv->size)
		pcv->size = pcv->size ? pcv->size * 2 : 1;
	void* ptr = realloc(pcv->data, pcv->size);
	if (!ptr)
		return 1;
	pcv->data = ptr;
	return 0;
}

int charvector_new(CharVector *pcv, size_t size) {
	if (!pcv)
		return 1;
	pcv->data = malloc(size);
	if (!pcv->data)
		return 1;
	pcv->length = 0;
	pcv->size = size;
	return 0;
}

int charvector_free(CharVector *pcv) {
	if (!pcv || !pcv->data)
		return 1;
	free(pcv->data);
	pcv->data = NULL;
	return 0;
}

int charvector_get(CharVector *pcv, size_t index, char *dst) {
	if (!pcv || !dst || index < 0 || index >= pcv->length)
		return 1;
	memcpy(dst, pcv->data + index, 1);
	return 0;
}

int charvector_pop(CharVector *pcv, char *dst) {
	if (!pcv || !dst || pcv->length == 0)
		return 1;
	memcpy(dst, pcv->data + pcv->length - 1, 1);
	pcv->length--;
	return 0;
}

int charvector_push(CharVector *pcv, char value) {
	if (!pcv)
		return 1;
	if (pcv->length >= pcv->size)
		if (_charvector_resize(pcv, 1))
			return 1;
	*(pcv->data + pcv->length) = value;
	pcv->length++;
	return 0;
}

int charvector_pusharray(CharVector *pcv, const char *src, size_t len) {
	if (!pcv || !src)
		return 1;
	if (pcv->length + len > pcv->size)
		if (_charvector_resize(pcv, len))
			return 1;
	memcpy(pcv->data + pcv->length, src, len);
	pcv->length += len;
	return 0;
}

int charvector_setarray(CharVector *pcv, size_t index, const char *src, size_t len) {
	if (!pcv || !src)
		return 1;
	if (index + len > pcv->size)
		if (_charvector_resize(pcv, len))
			return 1;
	memcpy(pcv->data + index, src, len);
	return 0;
}