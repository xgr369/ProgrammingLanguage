/*
* lang.h
*  Programming language API
*/

#ifndef LANG_H
#define LANG_H

#include "charlist.h"
#include "conf.h"
#include "stringhashtable.h"
#include "list.h"
#include "compiler.h"

typedef struct LangObject {
	struct LangObject *gcNext;
	char gcInfo;
} LangObject;

typedef struct LangState LangState;

#define LANG_TYPE_NULL		0
#define LANG_TYPE_NUMBER	1
#define LANG_TYPE_STRING	2
#define LANG_TYPE_CFUNCTION	4
#define LANG_TYPE_LFUNCTION 5
#define LANG_TYPE_TABLE		6
#define LANG_TYPE_USERDATA	7
#define LANG_TYPE_RANGE     8

typedef int (*lang_cfunction)(struct LangState *L);

typedef double lang_number;

typedef struct {
	int start;
	int end;
} lang_range;

typedef union {
	void *ptr;
	lang_number number;
	lang_range range;
} LangValue;

typedef struct {
	LangValue value;
	char type;
} LangTValue;

#define LANG_UPVAL_OPEN		0
#define LANG_UPVAL_CLOSED	1
#define LANG_UPVAL_DIRECT	0
#define LANG_UPVAL_INDIRECT	1

typedef struct {
	LangObject;
	char type;
	union {
		int index;
		LangTValue tvalue;
	};
} LangUpval;

typedef struct {
	LangObject;
	void *ptr;
	int numParam;
	int numUpval;
	LangUpval *upvalues[];
} LangFunction;

typedef struct {
	LangObject;
	int length;
	char data[];
} LangString;

typedef struct {
	LangObject;
} LangTable;

typedef struct {
	LangObject;
	char data[];
} LangUserdata;

#define LANG_STACK_BASE_SIZE 8
#define LANG_OK            0
#define LANG_ERROR_COMPILE 1
#define LANG_ERROR_RUN     2

typedef void (*LangWriteCallback)(const char *src, int length);

typedef struct {
	int stackFrame;
	int numReturnExpected;
	int pc;
	LangFunction *plfunction;
} LangCallInfo;

struct LangState {
	List stack; // List<LangTValue>
	LangCallInfo callInfo;
	List prevCallInfos; // List<LangCallInfo>
	StringHashTable importTable;
	LangObject *gcLow;
	LangObject *upvalOpen;
	LangObject *upvalClosed;
	LangC_CompilerState compilerState;
	int errorCode;
	const char *msg;
	lang_cfunction debug;
	lang_cfunction error;
};

#define lang_errmsg(L, _msg) {\
	L->errorCode = LANG_ERROR_RUN;\
	L->msg = _msg;\
}
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

LANG_API void		lang_binaryop		(LangState *L, char op);					// [-2, +1]
LANG_API void		lang_collectgarbage	(LangState *L);								// [-0, +0]
LANG_API void		lang_getfield		(LangState *L, const char *name, int len);	// [-1, +1]
LANG_API void		lang_getlocal		(LangState *L, int index);					// [-0, +1]
LANG_API void		lang_getupvalue		(LangState *L, int index);					// [-0, +1]
LANG_API void		lang_import			(LangState *L, const char *name);			// [-0, +1]
LANG_API void		lang_pop			(LangState *L);								// [-1, +0]
LANG_API void		lang_popn			(LangState *L, int n);						// [-n, +0]
LANG_API void		lang_pushlstring	(LangState *L, const char *str, int len);	// [-0, +1]
LANG_API void		lang_pushnull		(LangState *L);								// [-0, +1]
LANG_API void		lang_pushnumber		(LangState *L, lang_number value);			// [-0, +1]
LANG_API void		lang_pushrange		(LangState *L, int start, int end);			// [-0, +1]
LANG_API void		lang_pushthis		(LangState *L);								// [-0, +1]
LANG_API void		lang_pushtable		(LangState *L);								// [-0, +1]
LANG_API void		lang_removen		(LangState *L, int index, int n);			// [-n, +0]
LANG_API void		lang_setfield		(LangState *L, const char *name, int len);	// [-2, +0]
LANG_API void		lang_setlocal		(LangState *L, int index);					// [-1, +0]
LANG_API void		lang_setupvalue		(LangState *L, int index);					// [-1, +0]
LANG_API void		lang_tostring		(LangState *L);								// [-1, +1]
LANG_API void		lang_tonumber		(LangState *L);								// [-1, +1]
LANG_API void		lang_unaryop		(LangState *L, char op);					// [-1, +1]

void		lang_closeupvals	(LangState *L, int index);							// [-0, +0]
void		lang_precall		(LangState *L, int nArg, int nReturnExpected);		// [-0, +0]
void		lang_pushlfunction	(LangState *L, int src, int nParam, int nUpval,
									const char *upvals);							// [-0, +1]
void		lang_return 		(LangState *L, int nReturn);						// [-?, +0]
void		lang_tailcall		(LangState *L, int nArg);							// [-?, +0]

LANG_API LangTValue *lang_gettvalueglobal(LangState *L, int index);
LANG_API LangTValue *lang_gettvaluelocal(LangState *L, int index);
LANG_API int lang_iszero(LangState *L);
LANG_API void lang_registerfunc	(LangState *L, const char *name, lang_cfunction func);
LANG_API LangUserdata *lang_pushuserdata(LangState *L, const void *src, size_t sz);	// [-0, +1]
LANG_API int lang_tonumberbuf(const char *str, int len, double *out);
LANG_API int lang_tostringbuf(LangTValue *ptv, char *out);
LANG_API int lang_tostringbufd(double d, char *out);
LANG_API int lang_tostringbufi(int i, char *out);

LANG_API LangState *lang_newstate();
LANG_API void lang_atdebug(LangState *L, lang_cfunction debugf);
LANG_API void lang_aterror(LangState *L, lang_cfunction errorf);
LANG_API void lang_load(LangState *L, const char *src);
LANG_API void lang_close(LangState *L);


#endif // LANG_H