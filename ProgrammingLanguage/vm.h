/*
* vm.h
* Virtual machine
*/

#ifndef VM_H
#define VM_H

#include <string.h>
#include "lang.h"

typedef enum {
	LANGV_OP_BINARYOP,
	LANGV_OP_CALL,
	LANGV_OP_COPY,
	LANGV_OP_JMPNZ,
	LANGV_OP_LOADEXTERNVALUE,
	LANGV_OP_POPN,
	LANGV_OP_PUSHLSTRING,	    
	LANGV_OP_PUSHNIL,
	LANGV_OP_PUSHNUMBER,
	LANGV_OP_PUSHVALUE,
	LANGV_OP_REPLACE,
} LangV_Operation;

int langV_exec(LangState *ps, const char *pbc, int len);
int langV_dbg(const char *pbc, int len);

#endif // VM_H