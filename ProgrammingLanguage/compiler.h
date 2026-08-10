/*
* compiler.h
* Compiles abstract syntax tree into bytecode
*/

#ifndef COMPILER_H
#define COMPILER_H

#include "charlist.h"
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
	StringHashTable upvalTable; // StringHashTable<int for upvalues>
	List upvals; // List<LangC_UpvalDesc>
	int hasCapturedVariables;
} LangC_ScopeContext;

typedef struct {
	List scopeContexts; // List<LangC_ScopeContext *>
	const char *msg;
} LangC_CompilerState;

int langC_init(LangC_CompilerState *pcs);
int langC_compile(char *src, LangP_AstNode *root, LangC_CompilerState *pcs, char **dst, int *len);
void langC_free(LangC_CompilerState *pcs);

#endif // COMPILER_H