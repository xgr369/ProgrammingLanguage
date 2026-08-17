/*
* lang.h
* Programming language API
*/

#ifndef LANG_H
#define LANG_H

#include "conf.h"
#include "charlist.h"
#include "hash.h"
#include "list.h"

typedef struct LangState LangState;

#define LANG_TYPE_NULL		0
#define LANG_TYPE_NUMBER	1
#define LANG_TYPE_STRING	2
#define LANG_TYPE_FUNCTION	4
#define LANG_TYPE_USERDATA	7
#define LANG_TYPE_RANGE     8

#define LANG_VARIANT_LFUNC 0
#define LANG_VARIANT_CFUNC 1

#define LANG_OK          0
#define LANG_ERR_COMPILE 1
#define LANG_ERR_RUN     2
#define LANG_EXIT        3

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
	char type; char variant;
} LangTValue;

typedef struct LangObject {
	struct LangObject *gcNext;
	char gcInfo; char gcType;
} LangObject;

typedef struct {
	LangObject;
	char status;
	union {
		int index;
		LangTValue tv;
	} value;
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
	int dataSize; int numRefs;
	char data[];
	// LangObject * refs[] afterwards
} LangUserdata;

typedef struct {
	int stackFrame; int numReturnExpected; char *address;
	LangFunction *plfunction;
} LangCallInfo;

struct LangState {
	LangM_List stack; // LangM_List<LangTValue>
	LangCallInfo callInfo;
	LangM_List prevCallInfos; // LangM_List<LangCallInfo>
	char *src;
	int srcLen;
	char **paddress;
	int msgCode;
	const char *msg;
	LangM_Hash registry; // LangM_Hash<LangTValue>
	int gcLowCount; int gcLowThreshold;
	LangObject *gcLow;
	LangObject *upvalOpen;
	lang_cfunction debug;
	lang_cfunction error;
};

#define lang_errmsg(L, _msg) {\
	L->msgCode = LANG_ERR_RUN;\
	L->msg = _msg;\
}

// operation
LANG_API void lang_binaryop (LangState *L, char op);
LANG_API void lang_call (LangState *L, int nArg, int nReturnExpected);
LANG_API int  lang_iszero (LangState *L);
LANG_API void lang_tostring (LangState *L);
LANG_API void lang_tonumber (LangState *L);	
LANG_API void lang_unaryop	(LangState *L, char op);

// get
LANG_API void lang_getfield (LangState *L, const char *name, int len);
LANG_API void lang_getlocal	(LangState *L, int index);
LANG_API void lang_getupvalue (LangState *L, int index);
LANG_API void lang_import (LangState *L, const char *name);

// push
#define lang_pushliteral(L, l) (lang_pushlstring(L, "" l, sizeof(l)))
LANG_API void lang_pushlstring (LangState *L, const char *str, int len);
LANG_API void lang_pushnull (LangState *L);	
LANG_API void lang_pushnumber (LangState *L, lang_number value);
LANG_API void lang_pushrange (LangState *L, int start, int end);
LANG_API void lang_pushthis (LangState *L);	
LANG_API void lang_pushuserdata (LangState *L, int dataSize, int numRefs, void *srcData, LangObject **srcRef);

// set
LANG_API void lang_export (LangState *L, const char *name);
LANG_API void lang_setfield (LangState *L, const char *name, int len);
LANG_API void lang_setlocal (LangState *L, int index);
LANG_API void lang_setupvalue (LangState *L, int index);

// remove
LANG_API void lang_pop (LangState *L);
LANG_API void lang_popn	(LangState *L, int n);
LANG_API void lang_removen (LangState *L, int index, int n);

// misc
LANG_API void lang_registerfunc (LangState *L, const char *name, lang_cfunction func);
LANG_API void lang_collectgarbage (LangState *L);

// other
LANG_API LangTValue *lang_gettvalueglobal(LangState *L, int index);
LANG_API LangTValue *lang_gettvaluelocal(LangState *L, int index);
LANG_API void lang_pushtvalue(LangState *L, LangTValue *ptv);
LANG_API int lang_tonumberbuf(const char *str, int len, lang_number *out);
LANG_API int lang_tostringbuf(LangTValue *ptv, char *out);
LANG_API int lang_tostringbufd(double d, char *out);
LANG_API int lang_tostringbufi(int i, char *out);

LANG_API LangState *lang_newstate();
LANG_API void lang_atdebug(LangState *L, lang_cfunction debugf);
LANG_API void lang_aterror(LangState *L, lang_cfunction errorf);
LANG_API void lang_close(LangState *L);
LANG_API int lang_clear(LangState *L);
LANG_API void lang_load(LangState *L, const char *src);

#endif // LANG_H