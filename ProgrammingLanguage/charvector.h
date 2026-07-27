#ifndef CHARVECTOR_H
#define CHARVECTOR_H

#include <stdlib.h>

typedef struct {
	char* data;
	size_t length;
	size_t size;
} CharVector;

int charvector_new(CharVector *pcv, size_t size);
int charvector_free(CharVector *pcv);
int charvector_get(CharVector *pcv, size_t index, char *dst);
int charvector_pop(CharVector *pcv, char *dst);
int charvector_push(CharVector *pcv, char value);
int charvector_pusharray(CharVector *pcv, const char *src, size_t len);
int charvector_setarray(CharVector *pcv, size_t index, const char *src, size_t len);

#endif // CHARVECTOR_H