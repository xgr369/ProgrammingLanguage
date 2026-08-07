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
	void *pclosure;
} LangCallInfo;

typedef struct {
	LangCallInfo callInfo;
	StringHashTable importTable;
	LangObject *gcLow;
	const char *msg;
	List prevCallInfos; // List<LangCallInfo>
	List stack; // List<LangTValue>
	LangObject *upvalOpen;
	LangObject *upvalClosed;
	List upvalStack; // List<LangUpval *> // do we even need this?
} LangState;

#define LANG_TYPE_NULL		0
#define LANG_TYPE_NUMBER	1
#define LANG_TYPE_STRING	2
#define LANG_TYPE_CFUNCTION	4
#define LANG_TYPE_LCLOSURE  5
#define LANG_TYPE_TABLE		6
#define LANG_TYPE_USERDATA	7
#define LANG_TYPE_RANGE     8

typedef int (*lang_cfunction)(LangState *L);

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
#define LANG_UPVAL_NEW		2
#define LANG_UPVAL_OLD		3

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
} LangClosure;

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
void		lang_closeupvaluen	(LangState *L, int n);							// [-0, +0]
void		lang_collectgarbage	(LangState *L);									// [-0, +0]
void		lang_copy			(LangState *L, int idxFrom, int idxTo);			// [-0, +0]
void		lang_copytofield	(LangState *L, int idxFrom, char *name, int len); // [-0, +0]
void		lang_copytoupvalue	(LangState *L, int idxFrom, int idxTo);			// [-0, +0]
void		lang_field			(LangState *L, char *name, int len);			// [-1, +1]
void		lang_getlocal		(LangState *L, int index);						// [-0, +1]
void		lang_getupvalue		(LangState *L, int index);						// [-0, +1]
void		lang_import			(LangState *L, char *name);						// [-0, +1]
void		lang_pop			(LangState *L);									// [-1, +0]
void		lang_popn			(LangState *L, int n);							// [-n, +0]
void		lang_pushnull		(LangState *L);									// [-0, +1]
void		lang_pushnumber		(LangState *L, lang_number value);				// [-0, +1]
void		lang_pushlclosure	(LangState *L, size_t src, int nParam, int nUpval, char *upvals); // [-0, +1]
void		lang_pushlstring	(LangState *L, char *str, int len);				// [-0, +1]
void		lang_pushrange		(LangState *L, int start, int end);				// [-0, +1]
void		lang_pushtable		(LangState *L);									// [-0, +1]
void		lang_removen		(LangState *L, int index, int n);				// [-n, +0]
void		lang_setlocal		(LangState *L, int index);						// [-1, +0]
void		lang_setupvalue		(LangState *L, int index);						// [-1, +0]
void		lang_tostring		(LangState *L);									// [-1, +1]
void		lang_tonumber		(LangState *L);									// [-1, +1]
void		lang_unaryop		(LangState *L, char op);						// [-1, +1]

void		lang_precall		(LangState *L, int nArg, int nReturnExpected);	// [-0, +0]
void		lang_return 		(LangState *L, int nReturn);					// [-..., +0]
void		lang_tailcall		(LangState *L, int nArg);						// [-..., 0]

LangTValue *lang_gettvalueglobal(LangState *L, int index);
LangTValue *lang_gettvaluelocal	(LangState *L, int index);
int			lang_iszero			(LangState *L);									// [-0, +0]
void		lang_registerfunc	(LangState *L, const char *name, lang_cfunction func);
void		lang_pushuserdata(LangState *L, size_t sz);
int			lang_tonumberbuf	(const char *str, int len, double *out);
int			lang_tostringbufd	(double d, char *buf);
int			lang_tostringbufi	(int i, char *buf);

LANG_API void lang_compile(LangState *L, LangWriteCallback callback, char *src);
LANG_API LangState *lang_newstate();
LANG_API void lang_close(LangState *L);


#endif // LANG_H