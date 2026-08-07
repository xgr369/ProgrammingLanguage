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

static LangObject *newobject(LangState *L, size_t sz) {
	LangObject *plo = malloc(sz);
	if (!plo) {
		lang_errmsg(L, "newobject: malloc failed");
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
			case LANG_TYPE_NULL:
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
			LangString *pls = newobject(L, sizeof(LangString) + plsa->length + plsb->length);
			if (!pls) {
				return;
			}
			pls->length = plsa->length + plsb->length;
			memcpy(pls->data, plsa->data, plsa->length);
			memcpy(pls->data + plsa->length, plsb->data, plsb->length);

			LangTValue ltv;
			ltv.type = LANG_TYPE_STRING;
			ltv.value.ptr = pls;
			list_push(&L->stack, &ltv);
		} else {
			lang_errmsg(L, "lang_binaryop: unrecognized binary operation on strings");
		}
	} else {
		lang_errmsg(L, "lang_binaryop: binary operation on unrecognized type(s)");
	}
}

void lang_closeupvaluen(LangState *L, int n) {
	LangUpval *plu = L->upvalOpen;
	for (int i = 0; i < n; i++) {
		LangUpval *pluNext = plu->gcNext;
		LangTValue *pltv = gettvalueglobal(L, plu->index);
		plu->type = LANG_UPVAL_CLOSED;
		plu->tvalue = *pltv;
		plu->gcNext = L->upvalClosed;
		L->upvalClosed = plu;
		plu = pluNext;
	}
	L->upvalOpen = plu;
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

void lang_copy(LangState *L, int idxFrom, int idxTo) {
	LangTValue *pltv = gettvaluelocal(L, idxFrom);
	list_set(&L->stack, L->callInfo.stackFrame + idxTo, pltv);
}

void lang_copytofield(LangState *L, int idxFrom, char *name, int len) {
	lang_errmsg(L, "unimpl.");
}

void lang_copytoupvalue(LangState * L, int idxFrom, int idxTo) {
	if (L->callInfo.pclosure == NULL) {
		lang_errmsg(L, "lang_copytoupvalue: no upvalues");
		return;
	}
	LangClosure *pclosure = L->callInfo.pclosure;
	if (idxTo < 0 || idxTo >= pclosure->numUpval) {
		lang_errmsg(L, "lang_copytoupvalue: invalid upvalue index");
		return;
	}
	LangUpval *plu = pclosure->upvalues[idxTo];
	LangTValue *pltvFrom = gettvaluelocal(L, idxFrom);

	if (plu->type == LANG_UPVAL_OPEN) {
		LangTValue *pltvTo = gettvalueglobal(L, plu->index);
		*pltvTo = *pltvFrom;
	} else {
		plu->tvalue = *pltvFrom;
	}
}

void lang_field(LangState *L, char *name, int len) {
	lang_errmsg(L, "unimpl.");
}

void lang_getlocal(LangState *L, int index) {
	LangTValue *pltv = gettvaluelocal(L, index);
	list_push(&L->stack, pltv);
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
	LangUpval *plu = pclosure->upvalues[index];
	LangTValue *pltv;
	if (plu->type == LANG_UPVAL_OPEN) {
		pltv = gettvalueglobal(L, plu->index);
	} else {
		pltv = &plu->tvalue;
	}
	list_push(&L->stack, pltv);
}

void lang_import(LangState *L, const char *name) {
	LangTValue ltv;
	if (stringhashtable_get(&L->importTable, name, &ltv)) {
		lang_errmsg(L, "lang_import: value not found");
		return;
	}
	list_push(&L->stack, &ltv);
}

void lang_pop(LangState *L) {
	list_pop(&L->stack, NULL);
}

void lang_popn(LangState *L, int n) {
	list_popn(&L->stack, n);
}

void lang_precall(LangState *L, int nArg, int nReturnExpected) {
	LangTValue *pltv = gettvaluelocal(L, -nArg - 1);
	if (pltv->type == LANG_TYPE_LCLOSURE) {
		LangClosure *plc = pltv->value.ptr;
		if (nArg != plc->numParam) {
			lang_errmsg(L, "lang_call: wrong number of arguments");
			return;
		}
		list_push(&L->prevCallInfos, &L->callInfo);
		L->callInfo.pc = plc->ptr;
		L->callInfo.stackFrame = L->stack.length - nArg;
		L->callInfo.numReturnExpected = nReturnExpected;
		L->callInfo.pclosure = plc;
		return;
	}
	if (pltv->type == LANG_TYPE_CFUNCTION) {
		LangCallInfo callInfoPrev = L->callInfo;
		L->callInfo.stackFrame = L->stack.length - nArg;
		L->callInfo.pclosure = NULL;
		int nReturn = ((lang_cfunction)pltv->value.ptr)(L);
		if (nReturn < nReturnExpected) {
			for (int i = 0; i < nReturnExpected - nReturn; i++) {
				lang_pushnull(L);
			}
		} else if (nReturn > nReturnExpected) {
			list_popn(&L->stack, nReturn - nReturnExpected);
		}
		int nRemove = L->stack.length - nReturnExpected - (L->callInfo.stackFrame - 1);
		list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
		L->callInfo = callInfoPrev;
		return;
	}
	lang_errmsg(L, "lang_call: calling a non-lclosure/cfunction");
}


void lang_pushnull(LangState *L) {
	LangTValue ltv;
	ltv.type = LANG_TYPE_NULL;
	list_push(&L->stack, &ltv);
}

void lang_pushnumber(LangState *L, lang_number value) {
	LangTValue ltv;
	ltv.type = LANG_TYPE_NUMBER;
	ltv.value.number = value;
	list_push(&L->stack, &ltv);
}

void lang_pushlclosure(LangState *L, size_t src, int nParam, int nUpval, char *upvals) {
	LangClosure *plc = newobject(L, sizeof(LangClosure) + sizeof(LangUpval) * nUpval);
	if (!plc) {
		return;
	}
	plc->numParam = nParam;
	plc->numUpval = nUpval;
	plc->ptr = src;
	for (int i = 0; i < nUpval; i++) {
		char upvalType = *upvals;
		upvals++;

		int idx;
		memcpy(&idx, upvals, sizeof(int));
		upvals += sizeof(int);

		LangUpval *plu;
		if (upvalType == LANG_UPVAL_NEW) {
			plu = newobject(L, sizeof(LangUpval));
			if (!plu) {
				return;
			}
			plu->gcNext = L->upvalOpen;
			L->upvalOpen = plu;
			list_push(&L->upvalStack, &plu);
			plu->type = LANG_UPVAL_OPEN;
			plu->index = L->callInfo.stackFrame + idx;
		} else {
			LangClosure *pclosure = L->callInfo.pclosure;
			plu = pclosure->upvalues[idx];
		}
		plc->upvalues[i] = plu;
	}

	LangTValue ltv;
	ltv.type = LANG_TYPE_LCLOSURE;
	ltv.value.ptr = plc;
	list_push(&L->stack, &ltv);
}

void lang_pushlstring(LangState *L, const char *str, int len) {
	LangString *pls = newobject(L, sizeof(LangString) + len);
	if (!pls) {
		return;
	}
	pls->length = len;
	memcpy(pls->data, str, len);

	LangTValue ltv;
	ltv.type = LANG_TYPE_STRING;
	ltv.value.ptr = pls;
	list_push(&L->stack, &ltv);
}

void lang_pushrange(LangState *L, int start, int end) {
	LangTValue ltv;
	ltv.type = LANG_TYPE_RANGE;
	ltv.value.range.start = start;
	ltv.value.range.end = end;
	list_push(&L->stack, &ltv);
}

void lang_pushtable(LangState *L) {
	LangTable *plt = newobject(L, sizeof(LangTable));
	if (!plt) {
		return;
	}

	LangTValue ltv;
	ltv.type = LANG_TYPE_TABLE;
	ltv.value.ptr = plt;
	list_push(&L->stack, &ltv);
}

void lang_removen(LangState *L, int index, int n) {
	list_removen(&L->stack, L->callInfo.stackFrame + index, n);
}

void lang_return(LangState *L, int nReturn) {
	int nReturnExpected = L->callInfo.numReturnExpected;
	if (nReturn < nReturnExpected) {
		for (int i = 0; i < nReturnExpected - nReturn; i++) {
			lang_pushnull(L);
		}
	} else if (nReturn > nReturnExpected) {
		list_popn(&L->stack, nReturn - nReturnExpected);
	}
	int nRemove = L->stack.length - nReturnExpected - (L->callInfo.stackFrame - 1);
	list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
	list_pop(&L->prevCallInfos, &L->callInfo);
}

void lang_setlocal(LangState *L, int index) {
	LangTValue *pltv = gettvaluelocal(L, -1);
	list_set(&L->stack, L->callInfo.stackFrame + index, pltv);
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
	LangUpval *plu = pclosure->upvalues[index];
	LangTValue *pltvFrom = gettvaluelocal(L, -1);
	if (plu->type == LANG_UPVAL_OPEN) {
		LangTValue *pltvTo = gettvalueglobal(L, plu->index);
		*pltvTo = *pltvFrom;
	} else {
		plu->tvalue = *pltvFrom;
	}
	list_pop(&L->stack, NULL);
}

void lang_tailcall(LangState *L, int nArg) {
	LangTValue *pltv = gettvaluelocal(L, -nArg - 1);
	if (pltv->type == LANG_TYPE_LCLOSURE) {
		int nRemove = L->stack.length - nArg - L->callInfo.stackFrame;
		list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
		LangClosure *plc = pltv->value.ptr;
		L->callInfo.pc = plc->ptr;
		L->callInfo.pclosure = plc;
	} else if (pltv->type == LANG_TYPE_CFUNCTION) {
		int nRemove = L->stack.length - nArg - L->callInfo.stackFrame;
		list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
		L->callInfo.stackFrame = L->stack.length - nArg;
		L->callInfo.pclosure = NULL;
		int nReturn = ((lang_cfunction)pltv->value.ptr)(L);
		int nReturnExpected = L->callInfo.numReturnExpected;
		if (nReturn < nReturnExpected) {
			for (int i = 0; i < nReturnExpected - nReturn; i++) {
				lang_pushnull(L);
			}
		} else if (nReturn > nReturnExpected) {
			list_popn(&L->stack, nReturn - nReturnExpected);
		}
		nRemove = L->stack.length - nReturnExpected - (L->callInfo.stackFrame - 1);
		list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
		list_pop(&L->prevCallInfos, &L->callInfo);

	} else {
		lang_errmsg(L, "lang_tailcall: calling a non-lclosure/cfunction");
		return;
	}
}

void lang_tostring(LangState *L) {
	LangTValue *pltv = gettvaluelocal(L, -1);
	if (pltv->type == LANG_TYPE_STRING) {
		return;
	}
	list_pop(&L->stack, NULL);
	switch (pltv->type) {
		case LANG_TYPE_NULL:
			lang_pushliteral(L, "null");
			return;
		case LANG_TYPE_NUMBER:
		{
			char buf[64];
			int len;
			len = lang_tostringbufd(pltv->value.number, buf);
			lang_pushlstring(L, buf, len);
		} break;
		case LANG_TYPE_CFUNCTION:
		case LANG_TYPE_LCLOSURE:
		{
			char buf[64];
			int len;
			len = snprintf(buf, sizeof(buf), "<function %p>", pltv->value.ptr);
			lang_pushlstring(L, buf, len);
		} break;
		case LANG_TYPE_TABLE:
		{
			char buf[64];
			int len;
			len = snprintf(buf, sizeof(buf), "<table %p>", pltv->value.ptr);
			lang_pushlstring(L, buf, len);
		} break;
		default:
			lang_pushnull(L);
			break;
	}
}

void lang_tonumber(LangState *L) {
	LangTValue *pltv = gettvaluelocal(L, -1);
	if (pltv->type == LANG_TYPE_NUMBER || pltv->type == LANG_TYPE_NULL) {
		return;
	}
	list_pop(&L->stack, NULL);
	switch (pltv->type) {
		case LANG_TYPE_STRING:
		{
			lang_number d;
			LangString *pls = pltv->value.ptr;
			if (lang_tonumberbuf(pls->data, pls->length, &d)) {
				lang_pushnumber(L, d);
			} else {
				lang_pushnull(L);
			}
		} break;
		default:
			lang_pushnull(L);
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
			} else if (pltv->type == LANG_TYPE_NULL) {
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

LangTValue *lang_gettvalueglobal(LangState *L, int index) {
	return gettvalueglobal(L, index);
}

LangTValue *lang_gettvaluelocal(LangState *L, int index) {
	return gettvaluelocal(L, index);
}

int lang_iszero(LangState *L) {
	LangTValue *pltv = gettvaluelocal(L, -1);
	return pltv->type == LANG_TYPE_NULL || (pltv->type == LANG_TYPE_NUMBER && pltv->value.number == 0);
}

void lang_registerfunc(LangState *L, const char *name, lang_cfunction func) {
	LangTValue ltv;
	ltv.type = LANG_TYPE_CFUNCTION;
	ltv.value.ptr = func;
	stringhashtable_put(&L->importTable, name, &ltv);
}

void lang_pushuserdata(LangState *L, size_t sz) {
	LangUserdata *plo = newobject(L, sizeof(LangUserdata) + sz);

	LangTValue ltv;
	ltv.type = LANG_TYPE_USERDATA;
	ltv.value.ptr = plo;
	list_push(&L->stack, &ltv);
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

void lang_compile(LangState *L, LangWriteCallback callback, char *src) {
	LangP_LexerState ls;
	List tokens = langP_tokenize(src, &ls);
	if (ls.msg) {
		L->msg = ls.msg;
		list_free(&tokens);
		return;
	}
	LangP_ParserState prs;
	LangP_AstNode *ast = langP_parse(src, &tokens, &prs);
	if (!ast) {
		L->msg = prs.msg;
		list_free(&tokens);
		langP_free(ast);
		return;
	}
	LangC_CompilerState cs;
	CharList bc;
	if (charlist_new(&bc, 10)) {
		return;
	}
	if (langC_compile(src, ast, &cs, &bc)) {
		L->msg = cs.msg;
	} else {
		callback(bc.data, bc.length);
	}
	list_free(&tokens);
	langP_free(ast);
	langC_free(&cs);
	charlist_free(&bc);
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
	if (list_new(&L->prevCallInfos, sizeof(LangCallInfo), 1)) {
		return NULL;
	}
	if (list_new(&L->stack, sizeof(LangTValue), LANG_STACK_BASE_SIZE)) {
		return NULL;
	}
	L->upvalOpen = NULL;
	L->upvalClosed = NULL;
	if (list_new(&L->upvalStack, sizeof(LangUpval *), 1)) {
		return NULL;
	} // --test
	return L;
}

void lang_close(LangState *L) {
	stringhashtable_free(&L->importTable);
	list_free(&L->prevCallInfos);
	list_free(&L->stack);
}