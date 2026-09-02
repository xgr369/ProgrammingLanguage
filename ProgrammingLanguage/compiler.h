/*
* compiler.h
* Compiles abstract syntax tree into bytecode
*/

#ifndef COMPILER_H
#define COMPILER_H

#include "charlist.h"
#include "conf.h"
#include "parser.h"
#include "table.h"
#include "lang.h"

typedef enum {
	LANGC_SCOPE_TYPE_FUNCTION,
	LANGC_SCOPE_TYPE_NORMAL,
} LangC_ScopeType;

typedef struct {
	char type;
	int index;
} LangC_UpvalDesc;

typedef struct {
	LangM_Table identifierTable; // LangM_Table<int>
	size_t stackSize;
	LangC_ScopeType type;
	LangM_Table upvalTable; // LangM_Table<int for upvalues>
	LangM_List upvals; // LangM_List<LangC_UpvalDesc>
	int hasCapturedVariables;
} LangC_ScopeContext;

typedef struct {
	LangM_List scopeContexts; // LangM_List<LangC_ScopeContext *>
	const char *msg;
} LangC_CompilerState;

LangChunk langC_compile(const char *src, LangP_AstNode *ast, char **perrmsg);
void langC_free(LangChunk chunk);

#endif // COMPILER_H