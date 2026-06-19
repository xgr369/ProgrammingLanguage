/*
* compiler.h / langC
* Experimental bytecode compiler
*/

#ifndef COMPILER_H
#define COMPILER_H

#include "charvector.h"
#include "parser.h"
#include "stringhashtable.h"

typedef struct {
	StringHashTable identifierTable;
	size_t stackSize;
	char *msg;
} LangC_CompilerState;

static inline langC_emitop(CharVector *pbc, char c) {
	charvector_push(pbc, c);
}
#define langC_emitchar(pbc, c) (charvector_push(pbc, c))
#define langC_emitliteral(pbc, l) (charvector_pusharray(pbc, "" l, sizeof(l) - 1))
static inline langC_emitdouble(CharVector *pbc, double d) {
	charvector_pusharray(pbc, &d, sizeof(double));
}
static inline langC_emitinteger(CharVector *pbc, int i) {
	charvector_pusharray(pbc, &i, sizeof(int));
}
static inline langC_writeinteger(CharVector *pbc, int index, int i) {
	charvector_setarray(pbc, index, &i, sizeof(int));
}
static inline langC_emitlstring(CharVector *pbc, const char *src, int l) {
	charvector_pusharray(pbc, &l, sizeof(int));
	charvector_pusharray(pbc, src, l);
}
#define langC_errmsg(pc, _msg) (pc->msg = _msg)

int langC_compile(char *src, LangP_AstNode *root, LangC_CompilerState *pcs, CharVector *dst);

#endif // COMPILER_H