#ifndef JAGGEDVECTOR_H
#define JAGGEDVECTOR_H

#include <stdlib.h>
#include "charvector.h"
#include "vector.h"

#define BASIC_JAGGEDVECTOR_DATA_SIZE 1
#define BASIC_JAGGEDVECTOR_TABLE_SIZE 1

typedef struct {
	size_t index;
	size_t length;
} JaggedVectorTag;

typedef struct {
	CharVector buffer;
	Vector table; /* Vector<JaggedVectorTag> */
} JaggedVector;

int jaggedvector_new(JaggedVector *pjv);
int jaggedvector_get(JaggedVector *pjv, size_t index, void *dst, size_t elemSize);
int jaggedvector_pop(JaggedVector *pjv, void *dst, size_t elemSize);
int jaggedvector_push(JaggedVector *pjv, void *src, size_t elemSize);

#endif // JAGGEDVECTOR_H