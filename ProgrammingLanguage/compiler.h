/*
* compiler.h
* Compiles abstract syntax tree into bytecode
*/

#ifndef COMPILER_H
#define COMPILER_H

#include "charvector.h"
#include "conf.h"
#include "parser.h"
#include "stringhashtable.h"

typedef enum {
	LANGC_SCOPE_TYPE_FUNCTION,
	LANGC_SCOPE_TYPE_NORMAL,
} LangC_ScopeType;

typedef struct {
	char type;
	int index;
} LangC_UpvalDesc;

typedef struct {
	StringHashTable identifierTable; // StringHashTable<int>
	size_t stackSize;
	LangC_ScopeType type;
	StringHashTable upvalueTable; // StringHashTable<int for upvalues>
	Vector upvalues; // Vector<LangC_UpvalDesc>
} LangC_ScopeContext;

typedef struct {
	Vector scopeContexts; // Vector<LangC_ScopeContext>
	char *msg;
} LangC_CompilerState;

int langC_compile(char *src, LangP_AstNode *root, LangC_CompilerState *pcs, CharVector *dst);
void langC_free(LangC_CompilerState *pcs);

#endif // COMPILER_H