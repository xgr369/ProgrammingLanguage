/*
* vm.h
* Executes bytecode
*/

#ifndef VM_H
#define VM_H

#include <stdio.h>
#include <string.h>
#include "conf.h"
#include "core.h"
#include "compiler.h"

typedef enum {
	LANGV_OP_BINARYOP,
	LANGV_OP_CALL,
	LANGV_OP_CLOSEUPVALS,
	LANGV_OP_DEBUGGER,
	LANGV_OP_EXPORT,
	LANGV_OP_GETFIELD,
	LANGV_OP_GETLOCAL,
	LANGV_OP_GETUPVAL,
	LANGV_OP_IMPORT,
	LANGV_OP_JMP,
	LANGV_OP_JMPZ,
	LANGV_OP_LIST,
	LANGV_OP_POPN,
	LANGV_OP_PUSHFUNCTION,
	LANGV_OP_PUSHLSTRING,	    
	LANGV_OP_PUSHNULL,
	LANGV_OP_PUSHNUMBER,
	LANGV_OP_PUSHTHIS,
	LANGV_OP_RETURN,
	LANGV_OP_SETFIELD,
	LANGV_OP_SETLOCAL,
	LANGV_OP_SETUPVAL,
	LANGV_OP_TAILCALL,
	LANGV_OP_UNARYOP,
} LangV_Operation;

LANG_API int langV_exec(LangState *L, LangChunk chunk, int baseFrame);
LANG_API int langV_print(LangChunk chunk);

#endif // VM_H