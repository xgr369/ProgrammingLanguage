/*
* compiler.h / langC
* Experimental bytecode compiler
*/

#include "interpreter.h"
#include "charvector.h"

#ifndef COMPILER_H
#define COMPILER_H

#define langC_pushchar(pcv, c) (charvector_push(pcv, c))
#define langC_pushliteral(pcv, l) (charvector_pusharray(pcv, "" l, sizeof(l) - 1))

int langC_push(CharVector *pcv, const void *src, size_t len) {
	return charvector_pusharray(pcv, src, len);
}

#endif // COMPILER_H