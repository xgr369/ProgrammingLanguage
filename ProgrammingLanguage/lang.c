#include "lang.h"
#include <string.h>
#include "compiler.h"

#define LANG_MAX_NUMBER2STR 44

static inline LangTValue *gettvalueglobal(LangState *L, int index) {
	if (index < 0) {
		return L->stack.data + sizeof(LangTValue) * (L->stack.length + index);
	} else {
		return L->stack.data + sizeof(LangTValue) * (index);
	}
}

static inline LangTValue *gettvaluelocal(LangState *L, int index) {
	if (index < 0) {
		return L->stack.data + sizeof(LangTValue) * (L->stack.length + index);
	} else {
		return L->stack.data + sizeof(LangTValue) * (L->callInfo.stackFrame + index);
	}
}

static LangObject *lang_newobject(LangState *L, size_t sz) {
	LangObject *plo = malloc(sz);
	if (!plo) {
		lang_errmsg(L, "lang_newobject: malloc failed");
		return NULL;
	}
	plo->gcNext = L->gcLow;
	L->gcLow = plo;
	return plo;
}

void lang_binaryop(LangState *L, char op) {
	LangTValue *pa = gettvaluelocal(L, -2);
	LangTValue *pb = gettvaluelocal(L, -1);
	lang_popn(L, 2);
	if (op == LANG_OP_EQ) {
		if (pa->type != pb->type) {
			lang_pushnumber(L, 0);
			return;
		}
		switch (pa->type) {
			case LANG_TYPE_NIL:
				lang_pushnumber(L, 1);
				break;
			case LANG_TYPE_NUMBER:
				lang_pushnumber(L, pa->value.number == pb->value.number);
				break;
			case LANG_TYPE_STRING:
			{
				LangString *plsa = pa->value.ptr;
				LangString *plsb = pb->value.ptr;
				lang_pushnumber(L, plsa->length == plsb->length && strncmp(&plsa->data, &plsb->data, plsa->length) == 0);
			} break;
			default:
				lang_pushnumber(L, 0);
		}
	} else if (pa->type == LANG_TYPE_NUMBER && pb->type == LANG_TYPE_NUMBER) {
		switch (op) {
			case LANG_OP_ADD:
				lang_pushnumber(L, pa->value.number + pb->value.number);
				break;
			case LANG_OP_SUB:
				lang_pushnumber(L, pa->value.number - pb->value.number);
				break;
			case LANG_OP_MUL:
				lang_pushnumber(L, pa->value.number * pb->value.number);
				break;
			case LANG_OP_DIV:
				if (pb->value.number == 0) {
					lang_errmsg(L, "lang_binaryop: division by zero");
					return;
				}
				lang_pushnumber(L, pa->value.number / pb->value.number);
				break;
			case LANG_OP_LT:
				lang_pushnumber(L, pa->value.number < pb->value.number);
				break;
			case LANG_OP_GT:
				lang_pushnumber(L, pa->value.number > pb->value.number);
				break;
			case LANG_OP_LE:
				lang_pushnumber(L, pa->value.number <= pb->value.number);
				break;
			case LANG_OP_GE:
				lang_pushnumber(L, pa->value.number >= pb->value.number);
				break;
			default:
				lang_errmsg(L, "lang_binaryop: unrecognized binary operation on numbers");
		}
	} else if (pa->type == LANG_TYPE_STRING && pb->type == LANG_TYPE_STRING) {
		if (op == LANG_OP_ADD) {
			LangString *plsa = pa->value.ptr;
			LangString *plsb = pb->value.ptr;
			LangString *pls = lang_newobject(L, sizeof(LangString) + plsa->length + plsb->length);
			if (!pls) {
				return;
			}
			pls->length = plsa->length + plsb->length;
			memcpy(pls->data, plsa->data, plsa->length);
			memcpy(pls->data + plsa->length, plsb->data, plsb->length);

			LangTValue ltv;
			ltv.type = LANG_TYPE_STRING;
			ltv.value.ptr = pls;
			vector_push(&L->stack, &ltv);
		} else {
			lang_errmsg(L, "lang_binaryop: unrecognized binary operation on strings");
		}
	} else {
		lang_errmsg(L, "lang_binaryop: binary operation on unrecognized type(s)");
	}
}

/*
void lang_call(LangState *L, int nArg, int nReturnExpected) {
	LangTValue *pltv = gettvaluelocal(L, -nArg - 1);
	if (pltv->type == LANG_TYPE_LFUNCTION) {
		vector_push(&L->prevCallInfos, &L->callInfo);
		L->callInfo.pc = pltv->value.ptr;
		L->callInfo.stackFrame = L->stack.length - nArg;
		L->callInfo.numReturnExpected = nReturnExpected;
	} else if (pltv->type == LANG_TYPE_CFUNCTION) {
		int stackFramePrev = L->callInfo.stackFrame;
		L->callInfo.stackFrame = L->stack.length - nArg;
		int nReturn = ((lang_cfunction)pltv->value.ptr)(L);
		if (nReturn < nReturnExpected) {
			for (int i = 0; i < nReturnExpected - nReturn; i++) {
				lang_pushnil(L);
			}
		} else if (nReturn > nReturnExpected) {
			vector_popn(&L->stack, nReturn - nReturnExpected);
		}
		int nRemove = L->stack.length - nReturnExpected - (L->callInfo.stackFrame - 1);
		vector_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
		L->callInfo.stackFrame = stackFramePrev;
	} else {
		lang_errmsg(L, "calling a non-function");
	}
}*/

void lang_call(LangState *L, int nArg, int nReturnExpected) {
	LangTValue *pltv = gettvaluelocal(L, -nArg - 1);
	if (pltv->type == LANG_TYPE_LCLOSURE) {
		vector_push(&L->prevCallInfos, &L->callInfo);
		LangClosure *plc = pltv->value.ptr;
		L->callInfo.pc = plc->ptr;
		L->callInfo.stackFrame = L->stack.length - nArg;
		L->callInfo.numReturnExpected = nReturnExpected;
		L->callInfo.pclosure = plc;
	} else if (pltv->type == LANG_TYPE_CFUNCTION) {
		int stackFramePrev = L->callInfo.stackFrame;
		L->callInfo.stackFrame = L->stack.length - nArg;
		L->callInfo.pclosure = NULL; // not needed for cfunction
		int nReturn = ((lang_cfunction)pltv->value.ptr)(L);
		if (nReturn < nReturnExpected) {
			for (int i = 0; i < nReturnExpected - nReturn; i++) {
				lang_pushnil(L);
			}
		} else if (nReturn > nReturnExpected) {
			vector_popn(&L->stack, nReturn - nReturnExpected);
		}
		int nRemove = L->stack.length - nReturnExpected - (L->callInfo.stackFrame - 1);
		vector_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
		L->callInfo.stackFrame = stackFramePrev;
	} else {
		lang_errmsg(L, "lang_call: calling a non-lclosure/cfunction");
	}
}

// experimental GC through sweeping
// TODO: 1. improve; 2. decide when to call this
void lang_collectgarbage(LangState *L) {
	LangObject *plo = L->gcLow;
	while (plo) {
		plo->gcInfo = 0;
		plo = plo->gcNext;
	}
	for (int i = 0; i < L->stack.length; i++) {
		LangTValue *pltv = gettvalueglobal(L, i);
		if (pltv->type == LANG_TYPE_STRING) {
			LangObject *plo = pltv->value.ptr;
			plo->gcInfo = 1;
			
		}
	}

	LangObject **pplo = &L->gcLow;
	while (*pplo) {
		plo = *pplo;
		if (plo->gcInfo == 0) {
			*pplo = plo->gcNext;
			free(plo);
		} else {
			pplo = &plo->gcNext;
		}
	}
}

void lang_copy(LangState *L, int indexFrom, int indexTo) {
	LangTValue *pltv = gettvaluelocal(L, indexFrom);
	vector_set(&L->stack, indexTo, pltv);
}

void lang_getlocal(LangState *L, int index) {
	LangTValue *pltv = gettvaluelocal(L, index);
	vector_push(&L->stack, pltv);
}

void lang_getupvalue(LangState *L, int index) {
	if (L->callInfo.pclosure == NULL) {
		lang_errmsg(L, "lang_getupvalue: no upvalues");
		return;
	}
	LangClosure *pclosure = L->callInfo.pclosure;
	if (index < 0 || index >= pclosure->numUpval) {
		lang_errmsg(L, "lang_getupvalue: invalid upvalue index");
		return;
	}
	LangUpval lu = pclosure->upvalues[index];
	LangTValue *pltv = vector_at(&L->stack, lu.index);
	vector_push(&L->stack, pltv);
}

void lang_import(LangState *L, const char *name) {
	LangTValue ltv;
	if (stringhashtable_get(&L->importTable, name, &ltv)) {
		lang_errmsg(L, "lang_import: value not found");
		return;
	}
	vector_push(&L->stack, &ltv);
}

void lang_pop(LangState *L) {
	vector_pop(&L->stack, NULL);
}

void lang_popn(LangState *L, int n) {
	vector_popn(&L->stack, n);
}

void lang_pushnil(LangState *L) {
	LangTValue ltv;
	ltv.type = LANG_TYPE_NIL;
	vector_push(&L->stack, &ltv);
}

void lang_pushnumber(LangState *L, lang_number value) {
	LangTValue ltv;
	ltv.type = LANG_TYPE_NUMBER;
	ltv.value.number = value;
	vector_push(&L->stack, &ltv);
}

void lang_pushlclosure(LangState *L, size_t src, int nUpval, char *upvals) {
	LangClosure *plc = lang_newobject(L, sizeof(LangClosure) + sizeof(LangUpval) * nUpval);
	if (!plc) {
		return;
	}
	plc->numUpval = nUpval;
	plc->ptr = src;
	for (int i = 0; i < nUpval; i++) {
		char upvalType = upvals[i]; // ignored for now
		upvals++;

		int idx;
		memcpy(&idx, upvals, sizeof(int));
		upvals += sizeof(int);

		LangUpval lu;
		lu.index = idx;
		plc->upvalues[i] = lu;
	}

	LangTValue ltv;
	ltv.type = LANG_TYPE_LCLOSURE;
	ltv.value.ptr = plc;
	vector_push(&L->stack, &ltv);
}

void lang_pushlfunc(LangState *L, size_t src) {
	LangTValue ltv;
	ltv.type = LANG_TYPE_LFUNCTION;
	ltv.value.ptr = src;
	vector_push(&L->stack, &ltv);
}

void lang_pushlstring(LangState *L, const char *str, int len) {
	LangString *pls = lang_newobject(L, sizeof(LangString) + len);
	if (!pls) {
		return;
	}
	pls->length = len;
	memcpy(pls->data, str, len);

	LangTValue ltv;
	ltv.type = LANG_TYPE_STRING;
	ltv.value.ptr = pls;
	vector_push(&L->stack, &ltv);
}

void lang_removen(LangState *L, int index, int n) {
	vector_removen(&L->stack, L->callInfo.stackFrame + index, n);
}

void lang_return(LangState *L, int nReturn) {
	if (nReturn < L->callInfo.numReturnExpected) {
		for (int i = 0; i < L->callInfo.numReturnExpected - nReturn; i++) {
			lang_pushnil(L);
		}
	} else if (nReturn > L->callInfo.numReturnExpected) {
		vector_popn(&L->stack, nReturn - L->callInfo.numReturnExpected);
	}
	int nRemove = L->stack.length - L->callInfo.numReturnExpected - (L->callInfo.stackFrame - 1);
	vector_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
	vector_pop(&L->prevCallInfos, &L->callInfo);
}

void lang_setlocal(LangState *L, int index) {
	LangTValue *pltv = gettvaluelocal(L, -1);
	vector_set(&L->stack, L->callInfo.stackFrame + index, pltv);
	lang_pop(L);
}

void lang_setupvalue(LangState *L, int index) {
	if (L->callInfo.pclosure == NULL) {
		lang_errmsg(L, "lang_setupvalue: no upvalues");
		return;
	}
	LangClosure *pclosure = L->callInfo.pclosure;
	if (index < 0 || index >= pclosure->numUpval) {
		lang_errmsg(L, "lang_setupvalue: invalid upvalue index");
		return;
	}
	LangTValue *pltv = gettvaluelocal(L, -1);
	LangUpval lu = pclosure->upvalues[index];
	vector_set(&L->stack, lu.index, pltv);
}

void lang_tailcall(LangState *L, int nArg) {
	LangTValue *pltv = gettvaluelocal(L, -nArg - 1);
	if (pltv->type == LANG_TYPE_LCLOSURE) {
		LangClosure *plc = pltv->value.ptr;
		L->callInfo.pc = plc->ptr;
		L->callInfo.pclosure = plc;
		int nRemove = L->stack.length - nArg - L->callInfo.stackFrame;
		vector_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
	} else {
		lang_errmsg(L, "lang_tailcall: calling a non-lclosure");
		return;
	}
}

void lang_tostring(LangState *L) {
	LangTValue *pltv = gettvaluelocal(L, -1);
	if (pltv->type == LANG_TYPE_STRING) {
		return;
	}
	vector_pop(&L->stack, NULL);
	switch (pltv->type) {
		case LANG_TYPE_NIL:
			lang_pushliteral(L, "nil");
			return;
		case LANG_TYPE_NUMBER:
		{
			char buf[64];
			int len;
			len = lang_tostringbufd(pltv->value.number, buf);
			lang_pushlstring(L, buf, len);
		} break;
		case LANG_TYPE_CFUNCTION:
		case LANG_TYPE_LFUNCTION:
		case LANG_TYPE_LCLOSURE:
		{
			char buf[64];
			int len;
			len = snprintf(buf, sizeof(buf), "function: %p", pltv->value.ptr);
			lang_pushlstring(L, buf, len);
		} break;
		default:
			lang_pushnil(L);
			break;
	}
}

void lang_tonumber(LangState *L) {
	LangTValue *pltv = gettvaluelocal(L, -1);
	if (pltv->type == LANG_TYPE_NUMBER) {
		return;
	}
	vector_pop(&L->stack, NULL);
	switch (pltv->type) {
		case LANG_TYPE_NIL:
			lang_pushnumber(L, 0);
			break;
		case LANG_TYPE_STRING:
		{
			lang_number d;
			LangString *pls = pltv->value.ptr;
			if (lang_tonumberbuf(pls->data, pls->length, &d)) {
				lang_pushnumber(L, d);
			} else {
				lang_pushnil(L);
			}
		} break;
		default:
			lang_pushnil(L);
			break;
	}
}

void lang_unaryop(LangState *L, char op) {
	LangTValue *pltv = gettvaluelocal(L, -1);
	lang_pop(L);
	switch (op) {
		case LANG_OP_NEG:
		{
			if (pltv->type != LANG_TYPE_NUMBER) {
				lang_errmsg(L, "lang_unaryop: expected number in arithmetic negation");
				return;
			}
			lang_pushnumber(L, -pltv->value.number);
		} break;
		case LANG_OP_NOT:
		{
			if (pltv->type == LANG_TYPE_NUMBER) {
				lang_pushnumber(L, !pltv->value.number);
			} else if (pltv->type == LANG_TYPE_NIL) {
				lang_pushnumber(L, 1);
			} else {
				lang_pushnumber(L, 0);
			}
		} break;
		case LANG_OP_LEN:
		{
			if (pltv->type != LANG_TYPE_STRING) {
				lang_errmsg(L, "lang_unaryop: expected string in length operation");
				return;
			}
			LangString *pls = pltv->value.ptr;
			lang_pushnumber(L, pls->length);
		} break;
		default:
			lang_errmsg(L, "lang_unaryop: unrecognized unary operation");
			return;
	}
}

int lang_iszero(LangState *L) {
	LangTValue *pltv = gettvaluelocal(L, -1);
	return pltv->type == LANG_TYPE_NIL || (pltv->type == LANG_TYPE_NUMBER && pltv->value.number == 0);
}

int lang_tonumberbuf(const char *str, int len, double *out) {
	char buf[64];
	if (len >= sizeof(buf)) return 0;
	memcpy(buf, str, len);
	buf[len] = 0;
	char *end;
	*out = strtod(buf, &end);
	return end != buf && *end == 0;
}

int lang_tostringbufd(double d, char *buf) {
	return snprintf(buf, LANG_MAX_NUMBER2STR, "%f", d);
}

int lang_tostringbufi(int i, char *buf) {
	return snprintf(buf, LANG_MAX_NUMBER2STR, "%d", i);
}

LangTValue *lang_gettvalueglobal(LangState *L, int index) {
	return gettvalueglobal(L, index);
}

LangTValue *lang_gettvaluelocal(LangState *L, int index) {
	return gettvaluelocal(L, index);
}

void lang_registerfunc(LangState *L, const char *name, lang_cfunction func) {
	LangTValue ltv;
	ltv.type = LANG_TYPE_CFUNCTION;
	ltv.value.ptr = func;
	stringhashtable_put(&L->importTable, name, &ltv);
}

void lang_compile(LangState *L, LangWriteCallback callback, char *src) {
	LangP_LexerState ls;
	Vector tokens = langP_tokenize(src, &ls);
	if (ls.msg) {
		L->msg = ls.msg;
		vector_free(&tokens);
		return;
	}
	LangP_ParserState prs;
	LangP_AstNode *ast = langP_parse(src, &tokens, &prs);
	if (!ast) {
		L->msg = prs.msg;
		vector_free(&tokens);
		langP_free(ast);
		return;
	}
	LangC_CompilerState cs;
	CharVector bc;
	if (charvector_new(&bc, 10)) {
		return;
	}
	if (langC_compile(src, ast, &cs, &bc)) {
		L->msg = cs.msg;
		vector_free(&tokens);
		langP_free(ast);
		langC_free(&cs);
		charvector_free(&bc);
		return;
	}
	callback(bc.data, bc.length);
	vector_free(&tokens);
	langP_free(ast);
	langC_free(&cs);
	charvector_free(&bc);
	return;
}

LangState *lang_newstate() {
	LangState *L = malloc(sizeof(LangState));
	if (!L) {
		return NULL;
	}
	L->callInfo.stackFrame = 0;
	L->callInfo.pclosure = NULL;
	if (stringhashtable_new(&L->importTable, sizeof(LangTValue), 2)) {
		return NULL;
	}
	L->gcLow = NULL;
	L->msg = NULL;
	if (vector_new(&L->prevCallInfos, sizeof(LangCallInfo), 1)) {
		return NULL;
	}
	if (vector_new(&L->stack, sizeof(LangTValue), LANG_STACK_BASE_SIZE)) {
		return NULL;
	}
	return L;
}

void lang_close(LangState *L) {
	stringhashtable_free(&L->importTable);
	vector_free(&L->prevCallInfos);
	vector_free(&L->stack);
}