/*
* compiler.h
* Compiles abstract syntax tree into bytecode
*/

#ifndef COMPILER_H
#define COMPILER_H

#include "charlist.h"
#include "conf.h"
#include "parser.h"
#include "hash.h"

typedef enum {
	LANGC_SCOPE_TYPE_FUNCTION,
	LANGC_SCOPE_TYPE_NORMAL,
} LangC_ScopeType;

typedef struct {
	char type;
	int index;
} LangC_UpvalDesc;

typedef struct {
	LangM_Hash identifierTable; // LangM_Hash<int>
	size_t stackSize;
	LangC_ScopeType type;
	LangM_Hash upvalTable; // LangM_Hash<int for upvalues>
	LangM_List upvals; // LangM_List<LangC_UpvalDesc>
	int hasCapturedVariables;
} LangC_ScopeContext;

typedef struct {
	LangM_List scopeContexts; // LangM_List<LangC_ScopeContext *>
	const char *msg;
} LangC_CompilerState;

int langC_compile(char *src, LangP_AstNode *root, LangC_CompilerState *pcs, char **dst, int *len);
void langC_free(LangC_CompilerState *pcs);

#endif // COMPILER_H