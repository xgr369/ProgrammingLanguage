/*
* lang.h
*  Programming language API
*/

#ifndef LANG_H
#define LANG_H

#include "vector.h"
#include "stringhashtable.h"

#define LANG_STACK_BASE_SIZE 8

typedef struct {
	Vector stack; // Vector<LangTValue>
	StringHashTable externValueTable; // Vector<LangTValue>
	const char *msg;
} LangState;

#define LANG_TYPE_NIL		0
#define LANG_TYPE_NUMBER	1
#define LANG_TYPE_STRING	2
#define LANG_TYPE_FUNCTION	3

typedef double lang_number;
typedef int (*lang_cfunction) (LangState *ps);

typedef struct {
	size_t length;
	char data[];
} LangString;

typedef union {
	void *ptr;
	lang_number number;
	lang_cfunction cfunction;
} LangValue;

typedef struct {
	LangValue value;
	char type;
} LangTValue;

#define lang_errmsg(ps, _msg) (ps->msg = _msg)

int			lang_init_state		(LangState *ps);

#define LANG_OP_ADD		0
#define LANG_OP_SUB		1
#define LANG_OP_MUL	    2
#define LANG_OP_DIV		3
#define LANG_OP_EQ		4
#define LANG_OP_LT		5
#define LANG_OP_GT		6

void		lang_binaryop		(LangState *ps, char op);						// [-2, +1]

#define lang_pushliteral(ps, l) (lang_pushlstring(ps, "" l, sizeof(l)))			// [-0, +1]

void		lang_call			(LangState *ps, int nArg, int nReturn);
void		lang_copy			(LangState *ps, int indexFrom, int indexTo);	// [-0, +0]
void		lang_loadexternvalue(LangState *ps, const char *name);				// [-0, +1]
void		lang_pop			(LangState *ps);								// [-1, +0]
void		lang_popn			(LangState *ps, int n);							// [-n, +0]
void		lang_pushnil		(LangState *ps);								// [-0, +1]
void		lang_pushnumber		(LangState *ps, lang_number value);				// [-0, +1]
void		lang_pushlstring	(LangState *ps, const char *str, int len);		// [-0, +1]
void		lang_pushvalue		(LangState *ps, int index);						// [-0, +1]
void		lang_removen		(LangState *ps, int index, int n);				// [-n, +0]
void		lang_replace		(LangState *ps, int index);						// [-1, +0]
void		lang_storeexternvalue(LangState *ps, const char *name, lang_cfunction func);	// [-0, +0]

int			lang_isnonzero		(LangState *ps);								// [-0, +0]
lang_number lang_tonumber		(LangState *ps, int index);						// [-0, +0]
//lang_byte	lang_checkbyte		(LangState *ps, int index);						// [-0, +0]
//const lang_byte *lang_checklstring	(LangState *ps, int index, size_t *dstLen);		// [-0, +0]

#endif // LANG_H