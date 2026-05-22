#include "jaggedvector.h"

int _jaggedvector_resize(JaggedVector *pjv, size_t addLen) {
	while (pjv->buffer.length + addLen > pjv->buffer.size)
		pjv->buffer.size = pjv->buffer.size ? pjv->buffer.size * 2 : 1;
	void* ptr = realloc(pjv->buffer.data, pjv->buffer.size);
	if (!ptr)
		return 1;
	pjv->buffer.data = ptr;
	return 0;
}

int jaggedvector_new(JaggedVector *pjv) {
	if (!pjv || charvector_new(&pjv->buffer, BASIC_JAGGEDVECTOR_DATA_SIZE))
		return 1;
	if (vector_new(&pjv->table, sizeof(JaggedVectorTag), BASIC_JAGGEDVECTOR_TABLE_SIZE)) {
		charvector_free(&pjv->buffer);
		return 1;
	}
	return 0;
}

int jaggedvector_free(JaggedVector *pjv) {
	if (!pjv)
		return 1;
	vector_free(&pjv->table);
	charvector_free(&pjv->buffer);
}

int jaggedvector_get(JaggedVector* pjv, size_t index, void *dst, size_t elemSize) {
	if (!pjv || !dst)
		return 1;
	JaggedVectorTag tag;
	if (vector_get(&pjv->table, index, &tag))
		return 1;
	memcpy(dst, pjv->buffer.data + tag.index, elemSize);
	return 0;
}

int jaggedvector_pop(JaggedVector *pjv, void *dst, size_t elemSize) {
	if (!pjv || pjv->table.length == 0)
		return 1;
	JaggedVectorTag tag;
	if (vector_get(&pjv->table, pjv->table.length - 1, &tag))
		return 1;
	if (dst)
		memcpy(dst, pjv->buffer.data + tag.index, elemSize);
	pjv->buffer.length -= tag.length;
	pjv->table.length--;			
	return 0;
}

int jaggedvector_push(JaggedVector *pjv, void *src, size_t elemSize) {
	if (!pjv || !src)
		return 1;
	if (pjv->buffer.length + elemSize > pjv->buffer.size)
		if (_jaggedvector_resize(pjv, elemSize))
			return 1;
	memcpy(pjv->buffer.data + pjv->buffer.length, src, elemSize);
	JaggedVectorTag tag = { pjv->buffer.length, elemSize };
	if (vector_push(&pjv->table, &tag))
		return 1;
	pjv->buffer.length += elemSize;
	return 0;
}