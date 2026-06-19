#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

typedef struct {
	char *data;
	size_t elemSize;
	size_t length;
	size_t size;
} Vector;

int vector_new(Vector *pv, size_t elemSize, size_t size);
int vector_free(Vector *pv);
int vector_get(Vector *pv, size_t index, void *dst);
int vector_pop(Vector *pv, void *dst);
int vector_popn(Vector *pv, size_t n);
int vector_push(Vector *pv, const void *src);
int vector_removen(Vector *pv, size_t index, size_t n);
int vector_set(Vector *pv, size_t index, void *src);

#endif // VECTOR_H