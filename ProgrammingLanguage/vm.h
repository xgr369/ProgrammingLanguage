/*
* vm.h
* Executes bytecode
*/

#ifndef VM_H
#define VM_H

#include <stdio.h>
#include <string.h>
#include "conf.h"
#include "lang.h"

typedef enum {
	LANGV_OP_BINARYOP,
	LANGV_OP_CALL,
	LANGV_OP_CLOSEUPVALN,
	LANGV_OP_END,
	LANGV_OP_FIELD,
	LANGV_OP_GETLOCAL,
	LANGV_OP_GETUPVAL,
	LANGV_OP_JMP,
	LANGV_OP_JMPZ,
	LANGV_OP_IMPORT,
	LANGV_OP_MOVE,
	LANGV_OP_MOVETOFIELD,
	LANGV_OP_MOVETOUPVAL,
	LANGV_OP_POPN,
	LANGV_OP_PUSHLCLOSURE,
	LANGV_OP_PUSHLSTRING,	    
	LANGV_OP_PUSHNULL,
	LANGV_OP_PUSHNUMBER,
	LANGV_OP_RETURN,
	LANGV_OP_SETLOCAL,
	LANGV_OP_SETUPVAL,
	LANGV_OP_TAILCALL,
	LANGV_OP_UNARYOP,
} LangV_Operation;

LANG_API langV_exec(LangState *L, const char *pbc, int len);
LANG_API langV_print(LangWriteCallback callback, const char *pbc, int len);

#endif // VM_H