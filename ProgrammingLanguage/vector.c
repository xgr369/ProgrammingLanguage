#include "vector.h"
#include <string.h>

int _vector_resize(Vector *pv) {
	pv->size = pv->size ? pv->size * 2 : 1;
	void* ptr = realloc(pv->data, pv->size * pv->elemSize);
	if (!ptr)
		return 1;
	pv->data = ptr;
	return 0;
}

int vector_new(Vector *pv, size_t elemSize, size_t size) {
	if (!pv || elemSize == 0)
		return 1;
	pv->data = malloc(size * elemSize);
	if (!pv->data)
		return 1;
	pv->length = 0;
	pv->size = size;
	pv->elemSize = elemSize;
	return 0;
}

int vector_free(Vector* pv) {
	if (!pv || !pv->data)
		return 1;
	free(pv->data);
	pv->data = NULL;
	return 0;
}

int vector_get(Vector* pv, size_t index, void *dst) {
	if (!pv || !dst || index < 0 || index >= pv->length)
		return 1;
	memcpy(dst, pv->data + index * pv->elemSize, pv->elemSize);
	return 0;
}

int vector_pop(Vector *pv, void *dst) {
	if (!pv || pv->length == 0)
		return 1;
	if (dst)
		memcpy(dst, pv->data + (pv->length * pv->elemSize), pv->elemSize);
	pv->length--;
	return 0;
}

int vector_push(Vector *pv, const void *src) {
	if (!pv || !src)
		return 1;
	if (pv->length * pv->elemSize >= pv->size)
		if (_vector_resize(pv))
			return 1;
	memcpy(pv->data + (pv->length * pv->elemSize), src, pv->elemSize);
	pv->length++;
	return 0;
}