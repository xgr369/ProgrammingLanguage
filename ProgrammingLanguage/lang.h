/*
* lang.h
*  Programming language API
*/

#ifndef LANG_H
#define LANG_H

#include "charvector.h"
#include "conf.h"
#include "stringhashtable.h"
#include "vector.h"

typedef struct LangObject {
	struct LangObject *gcNext;
	char gcInfo;
} LangObject;

#define LANG_STACK_BASE_SIZE 8

typedef void (*LangWriteCallback)(char *src, size_t length);

typedef struct {
	int stackFrame;
	size_t pc;
	int numReturnExpected;
	void *pclosure; // test
} LangCallInfo;

typedef struct {
	LangCallInfo callInfo;
	StringHashTable importTable;
	LangObject *gcLow;
	const char *msg;
	Vector prevCallInfos; // Vector<LangCallInfo>
	Vector stack; // Vector<LangTValue>
} LangState;

#define LANG_TYPE_NIL		0
#define LANG_TYPE_NUMBER	1
#define LANG_TYPE_STRING	2
#define LANG_TYPE_LFUNCTION	3
#define LANG_TYPE_CFUNCTION	4
#define LANG_TYPE_LCLOSURE  5

typedef double lang_number;

typedef int (*lang_cfunction)(LangState *L);

typedef struct {
	LangObject;
	size_t length;
	char data[];
} LangString;

typedef union {
	void *ptr;
	lang_number number;
} LangValue;

typedef struct {
	LangValue value;
	char type;
} LangTValue;

typedef struct {
	int index;
} LangUpval;

typedef struct {
	LangObject;
	void *ptr;
	int numUpval;
	LangUpval upvalues[];
} LangClosure;

#define lang_errmsg(L, _msg) (L->msg = _msg)
#define lang_pushliteral(L, l) (lang_pushlstring(L, "" l, sizeof(l)))			// [-0, +1]

#define LANG_OP_ADD		0
#define LANG_OP_SUB		1
#define LANG_OP_MUL	    2
#define LANG_OP_DIV		3
#define LANG_OP_EQ		4
#define LANG_OP_LT		5
#define LANG_OP_GT		6
#define LANG_OP_LE		7
#define LANG_OP_GE		8
#define LANG_OP_NEG		9
#define LANG_OP_NOT		10
#define LANG_OP_LEN		11

void		lang_binaryop		(LangState *L, char op);						// [-2, +1]
void		lang_call			(LangState *L, int nArg, int nReturnExpected);  // [-0, +0] internal
void		lang_collectgarbage	(LangState *L);									// [-0, +0]
void		lang_copy			(LangState *L, int indexFrom, int indexTo);		// [-0, +0]
void		lang_getlocal		(LangState *L, int index);						// [-0, +1]
void		lang_getupvalue		(LangState *L, int index);						// [-0, +1]
void		lang_import			(LangState *L, const char *name);				// [-0, +1]
void		lang_pop			(LangState *L);									// [-1, +0]
void		lang_popn			(LangState *L, int n);							// [-n, +0]
void		lang_pushnil		(LangState *L);									// [-0, +1]
void		lang_pushnumber		(LangState *L, lang_number value);				// [-0, +1]
void		lang_pushlclosure	(LangState *L, size_t src, int nUpval, char *upvals); // [-0, +1]
void		lang_pushlfunc		(LangState *L, size_t src);						// [-0, +1]
void		lang_pushlstring	(LangState *L, const char *str, int len);		// [-0, +1]
void		lang_removen		(LangState *L, int index, int n);				// [-n, +0]
void		lang_return 		(LangState *L, int nReturn);					// [-..., +0] internal
void		lang_setlocal		(LangState *L, int index);						// [-1, +0]
void		lang_setupvalue		(LangState *L, int index);						// [-1, +0]
void		lang_tailcall		(LangState *L, int nArg);						// [-..., 0] internal
void		lang_tostring		(LangState *L);									// [-1, +1]
void		lang_tonumber		(LangState *L);									// [-1, +1]
void		lang_unaryop		(LangState *L, char op);						// [-1, +1]

int			lang_iszero			(LangState *L);									// [-0, +0]
int			lang_tonumberbuf	(const char *str, int len, double *out);
int			lang_tostringbufd	(double d, char *buf);
int			lang_tostringbufi	(int i, char *buf);
LangTValue *lang_gettvalueglobal(LangState *L, int index);
LangTValue *lang_gettvaluelocal	(LangState *L, int index);
void		lang_registerfunc	(LangState *L, const char *name, lang_cfunction func);

LANG_API void lang_compile(LangState *L, LangWriteCallback callback, char *src);
LANG_API LangState *lang_newstate();
LANG_API void lang_close(LangState *L);


#endif // LANG_H