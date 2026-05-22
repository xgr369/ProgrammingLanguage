/*
* interpreter.h / langI
* Experimental bytecode interpreter
*/

#include <string.h>
#include "lang.h"

#ifndef INTERPRETER_H
#define INTERPRETER_H

typedef enum {				// Operands
	LANG_I_OP_PUSHCHAR,		// char c						
	LANG_I_OP_PUSHLSTRING,	// size_t len, char [...len]    
	LANG_I_OP_ARITH,		// char op
} InterpreterOperation;

int langI_exec(LangState *ps, const char *pbc, int len) {
	size_t pc = 0;
	while (pc < len) {
		//printf("pc=%ld, op=%ld\n", pc, pbc[pc]);
		switch (pbc[pc]) {
			case LANG_I_OP_PUSHCHAR:
			{
				pc++;

				char c = pbc[pc];
				pc++;

				lang_pushchar(ps, c);
				break;
			}
			case LANG_I_OP_PUSHLSTRING:
			{
				pc++;

				size_t strLen;
				memcpy(&strLen, pbc + pc, sizeof(size_t));
				pc += sizeof(size_t);

				const char *pstr = pbc + pc;
				pc += strLen;

				lang_pushlstring(ps, pstr, strLen);
				break;
			}
			case LANG_I_OP_ARITH:
			{
				pc++;

				char op = pbc[pc];
				pc++;

				lang_arith(ps, op);
				break;
			}
			default:
				printf("[ERROR] pc=%ld, op=%ld\n", pc, pbc[pc]);
				lang_errmsg(ps, "unrecognized bytecode operation");
				return 1;
		}
	}
	return 0;
}

#endif // INTERPRETER_H