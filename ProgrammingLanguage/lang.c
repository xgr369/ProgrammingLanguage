#include "lang.h"
#include <string.h>
#include "compiler.h"

#define LANG_MAX_TOSTRING_LEN 44

static LangTValue *gettvalueglobal(LangState *L, int index) {
	if (index < 0) {
		return L->stack.data + sizeof(LangTValue) * (L->stack.length + index);
	} else {
		return L->stack.data + sizeof(LangTValue) * (index);
	}
}

static LangTValue *gettvaluelocal(LangState *L, int index) {
	if (index < 0) {
		return L->stack.data + sizeof(LangTValue) * (L->stack.length + index);
	} else {
		return L->stack.data + sizeof(LangTValue) * (L->callInfo.stackFrame + index);
	}
}

static LangTValue *gettvaluethis(LangState *L) {
	return L->stack.data + sizeof(LangTValue) * (L->callInfo.stackFrame - 1);
}

static LangObject *newobject(LangState *L, size_t sz) {
	LangObject *po = malloc(sz);
	if (!po) {
		lang_errmsg(L, "allocation failed");
		return NULL;
	}
	po->gcNext = L->gcLow;
	L->gcLow = po;
	return po;
}

static LangUpval *findupval(LangState *L, int index) {
	int idxGlobal = L->callInfo.stackFrame + index;
	LangUpval **ppu = &L->upvalOpen;
	while (*ppu != NULL && (*ppu)->index > idxGlobal) {
		ppu = &(*ppu)->gcNext;
	}
	if (*ppu != NULL && (*ppu)->index == idxGlobal) {
		return *ppu;
	}
	LangUpval *pu = newobject(L, sizeof(LangUpval));
	if (!pu) {
		return NULL;
	}
	pu->type = LANG_UPVAL_OPEN;
	pu->index = idxGlobal;

	pu->gcNext = *ppu;
	*ppu = pu;
	return pu;
}

static int no_cfunction(LangState *L) {
	return 0;
}

void lang_binaryop(LangState *L, char op) {
	LangTValue *pa = gettvaluelocal(L, -2);
	LangTValue *pb = gettvaluelocal(L, -1);
	LangTValue result;
	if (op == LANG_OP_EQ) {
		result.type = LANG_TYPE_NUMBER;
		if (pa->type != pb->type) {
			result.value.number = 0;
			goto lang_binaryop_push;
		}
		switch (pa->type) {
			case LANG_TYPE_NULL:
				result.value.number = 1;
				break;
			case LANG_TYPE_NUMBER:
				result.value.number = pa->value.number == pb->value.number;
				break;
			case LANG_TYPE_STRING:
			{
				LangString *psa = pa->value.ptr;
				LangString *psb = pb->value.ptr;
				result.value.number = psa->length == psb->length && strncmp(&psa->data, &psb->data, psa->length) == 0;
			} break;
			default:
				result.value.number = 0;
		}
	} else if (op == LANG_OP_ADD) {
		int isString = pa->type == LANG_TYPE_STRING || pb->type == LANG_TYPE_STRING;
		if (isString) {
			// TODO:
			// in the future, maybe turn this into multiple value concatenation (also do it for addition)
			// e.g. a + b + c --> +(a, +(b, c))
			// compile_expr(a); compile_expr(b); compile_expr(c); emitop(OP_ADDN); emitinteger(3);
			char bufa[LANG_MAX_TOSTRING_LEN], bufb[LANG_MAX_TOSTRING_LEN];
			char *ptra, *ptrb;
			int lena, lenb;
			if (pa->type == LANG_TYPE_STRING) {
				LangString *ps = ((LangString *)pa->value.ptr);
				lena = ps->length;
				ptra = ps->data;
			} else {
				lena = lang_tostringbuf(pa, bufa);
				ptra = bufa;
			}			
			if (pb->type == LANG_TYPE_STRING) {
				LangString *ps = ((LangString *)pb->value.ptr);
				lenb = ps->length;
				ptrb = ps->data;
			} else {
				lenb = lang_tostringbuf(pb, bufb);
				ptrb = bufb;
			}
			LangString *ps = newobject(L, sizeof(LangString) + lena + lenb);
			if (!ps) {
				return;
			}
			ps->length = lena + lenb;
			memcpy(ps->data, ptra, lena);
			memcpy(ps->data + lena, ptrb, lenb);
			result.type = LANG_TYPE_STRING;
			result.value.ptr = ps;
		} else if (pa->type == LANG_TYPE_NUMBER && pb->type == LANG_TYPE_NUMBER) {
			result.type = LANG_TYPE_NUMBER;
			result.value.number = pa->value.number + pb->value.number;
		} else {
			lang_errmsg(L, "invalid operand(s)");
			return;
		}
	} else {
		if (pa->type != LANG_TYPE_NUMBER || pb->type != LANG_TYPE_NUMBER) {
			lang_errmsg(L, "invalid operand(s)");
			return;
		}
		result.type = LANG_TYPE_NUMBER;
		switch (op) {
			case LANG_OP_SUB:
				result.value.number = pa->value.number - pb->value.number;
				break;
			case LANG_OP_MUL:
				result.value.number = pa->value.number * pb->value.number;
				break;
			case LANG_OP_DIV:
				if (pb->value.number == 0) {
					lang_errmsg(L, "division by zero");
					return;
				}
				result.value.number = pa->value.number / pb->value.number;
				break;
			case LANG_OP_LT:
				result.value.number = pa->value.number < pb->value.number;
				break;
			case LANG_OP_GT:
				result.value.number = pa->value.number > pb->value.number;
				break;
			case LANG_OP_LE:
				result.value.number = pa->value.number <= pb->value.number;
				break;
			case LANG_OP_GE:
				result.value.number = pa->value.number >= pb->value.number;
				break;
			default:
				assert(0);
				return;
		}
	}
lang_binaryop_push:
	list_popn(&L->stack, 2);
	list_push(&L->stack, &result);
}

// experimental GC through sweeping
// TODO: 1. improve; 2. decide when to call this
void lang_collectgarbage(LangState *L) {
	LangObject *po = L->gcLow;
	while (po) {
		po->gcInfo = 0;
		po = po->gcNext;
	}
	for (int i = 0; i < L->stack.length; i++) {
		LangTValue *ptv = gettvalueglobal(L, i);
		if (ptv->type == LANG_TYPE_STRING) {
			LangObject *po = ptv->value.ptr;
			po->gcInfo = 1;
			
		}
	}

	LangObject **ppo = &L->gcLow;
	while (*ppo) {
		po = *ppo;
		if (po->gcInfo == 0) {
			*ppo = po->gcNext;
			free(po);
		} else {
			ppo = &po->gcNext;
		}
	}
}

void lang_closeupvals(LangState * L, int index) {
	LangUpval **ppu = &L->upvalOpen;
	while (*ppu != NULL && (*ppu)->index >= index) {
		LangUpval *pu = *ppu;
		LangTValue *ptv = gettvalueglobal(L, pu->index);
		pu->type = LANG_UPVAL_CLOSED;
		pu->tvalue = *ptv;

		pu->gcNext = L->upvalClosed;
		L->upvalClosed = pu;

		ppu = &(*ppu)->gcNext;
	}
	L->upvalOpen = *ppu;
}

void lang_getfield(LangState *L, const char *name, int len) {
	lang_errmsg(L, "unimplemented functionality");
}

void lang_getlocal(LangState *L, int index) {
	LangTValue *ptv = gettvaluelocal(L, index);
	list_push(&L->stack, ptv);
}

void lang_getupvalue(LangState *L, int index) {
	if (L->callInfo.plfunction == NULL) {
		assert(0);
		return;
	}
	LangFunction *plf = L->callInfo.plfunction;
	if (index < 0 || index >= plf->numUpval) {
		assert(0);
		return;
	}
	LangUpval *pu = plf->upvalues[index];
	LangTValue *ptv;
	if (pu->type == LANG_UPVAL_OPEN) {
		ptv = gettvalueglobal(L, pu->index);
	} else {
		ptv = &pu->tvalue;
	}
	list_push(&L->stack, ptv);
}

void lang_import(LangState *L, const char *name) {
	LangTValue tv;
	if (stringhashtable_get(&L->importTable, name, &tv)) {
		lang_errmsg(L, "import value not found");
		return;
	}
	list_push(&L->stack, &tv);
}

void lang_pop(LangState *L) {
	list_pop(&L->stack, NULL);
}

void lang_popn(LangState *L, int n) {
	list_popn(&L->stack, n);
}

void lang_precall(LangState *L, int nArg, int nReturnExpected) {
	LangTValue *ptv = gettvaluelocal(L, -nArg - 1);
	if (ptv->type == LANG_TYPE_LFUNCTION) {
		LangFunction *plf = ptv->value.ptr;
		if (nArg != plf->numParam) {
			lang_errmsg(L, "invalid number of arguments");
			return;
		}
		list_push(&L->prevCallInfos, &L->callInfo);
		L->callInfo.pc = plf->ptr;
		L->callInfo.stackFrame = L->stack.length - nArg;
		L->callInfo.numReturnExpected = nReturnExpected;
		L->callInfo.plfunction = plf;
		return;
	}
	if (ptv->type == LANG_TYPE_CFUNCTION) {
		LangCallInfo callInfoPrev = L->callInfo;
		L->callInfo.stackFrame = L->stack.length - nArg;
		L->callInfo.plfunction = NULL;
		int nReturn = ((lang_cfunction)ptv->value.ptr)(L);
		if (nReturn < nReturnExpected) {
			for (int i = 0; i < nReturnExpected - nReturn; i++) {
				lang_pushnull(L);
			}
		} else if (nReturn > nReturnExpected) {
			list_popn(&L->stack, nReturn - nReturnExpected);
		}
		lang_closeupvals(L, L->callInfo.stackFrame);
		int nRemove = L->stack.length - nReturnExpected - (L->callInfo.stackFrame - 1);
		list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
		L->callInfo = callInfoPrev;
		return;
	}
	lang_errmsg(L, "bad invocation");
}

void lang_pushlfunction(LangState *L, int src, int nParam, int nUpval, const char *upvals) {
	LangFunction *plf = newobject(L, sizeof(LangFunction) + sizeof(LangUpval) * nUpval);
	if (!plf) {
		return;
	}
	plf->numParam = nParam;
	plf->numUpval = nUpval;
	plf->ptr = src;
	for (int i = 0; i < nUpval; i++) {
		char upvalType = *upvals;
		upvals++;

		int idx;
		memcpy(&idx, upvals, sizeof(int));
		upvals += sizeof(int);

		LangUpval *pu;
		if (upvalType == LANG_UPVAL_DIRECT) {
			pu = findupval(L, idx);
			if (!pu) {
				return;
			}
		} else if (upvalType == LANG_UPVAL_INDIRECT) {
			LangFunction *plfunction = L->callInfo.plfunction;
			pu = plfunction->upvalues[idx];
		} else {
			assert(0);
			return;
		}
		plf->upvalues[i] = pu;
	}

	LangTValue tv;
	tv.type = LANG_TYPE_LFUNCTION;
	tv.value.ptr = plf;
	list_push(&L->stack, &tv);
}

void lang_pushlstring(LangState *L, const char *str, int len) {
	LangString *ps = newobject(L, sizeof(LangString) + len);
	if (!ps) {
		return;
	}
	ps->length = len;
	memcpy(ps->data, str, len);

	LangTValue tv;
	tv.type = LANG_TYPE_STRING;
	tv.value.ptr = ps;
	list_push(&L->stack, &tv);
}

void lang_pushnull(LangState *L) {
	LangTValue tv;
	tv.type = LANG_TYPE_NULL;
	list_push(&L->stack, &tv);
}

void lang_pushnumber(LangState *L, lang_number value) {
	LangTValue tv;
	tv.type = LANG_TYPE_NUMBER;
	tv.value.number = value;
	list_push(&L->stack, &tv);
}

void lang_pushrange(LangState *L, int start, int end) {
	LangTValue tv;
	tv.type = LANG_TYPE_RANGE;
	tv.value.range.start = start;
	tv.value.range.end = end;
	list_push(&L->stack, &tv);
}

void lang_pushthis(LangState *L) {
	if (L->callInfo.plfunction == NULL) {
		lang_errmsg(L, "this not in function");
		return;
	}
	LangTValue *ptv = gettvaluethis(L);
	list_push(&L->stack, ptv);
}

void lang_pushtable(LangState *L) {
	LangTable *pt = newobject(L, sizeof(LangTable));
	if (!pt) {
		return;
	}

	LangTValue tv;
	tv.type = LANG_TYPE_TABLE;
	tv.value.ptr = pt;
	list_push(&L->stack, &tv);
}

void lang_removen(LangState *L, int index, int n) {
	list_removen(&L->stack, L->callInfo.stackFrame + index, n);
}

void lang_setfield(LangState *L, const char *name, int len) {
	lang_errmsg(L, "unimplemented functionality");
}

void lang_return(LangState *L, int nReturn) {
	if (L->callInfo.plfunction == NULL) {
		lang_errmsg(L, "return not in function");
		return;
	}
	int nReturnExpected = L->callInfo.numReturnExpected;
	if (nReturn < nReturnExpected) {
		for (int i = 0; i < nReturnExpected - nReturn; i++) {
			lang_pushnull(L);
		}
	} else if (nReturn > nReturnExpected) {
		list_popn(&L->stack, nReturn - nReturnExpected);
	}
	lang_closeupvals(L, L->callInfo.stackFrame);
	int nRemove = L->stack.length - nReturnExpected - (L->callInfo.stackFrame - 1);
	list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
	list_pop(&L->prevCallInfos, &L->callInfo);
}

void lang_setlocal(LangState *L, int index) {
	LangTValue *ptv = gettvaluelocal(L, -1);
	list_set(&L->stack, L->callInfo.stackFrame + index, ptv);
	list_pop(&L->stack, NULL);
}

void lang_setupvalue(LangState *L, int index) {
	if (L->callInfo.plfunction == NULL) {
		assert(0);
		return;
	}
	LangFunction *plf = L->callInfo.plfunction;
	if (index < 0 || index >= plf->numUpval) {
		assert(0);
		return;
	}
	LangUpval *pu = plf->upvalues[index];
	LangTValue *ptvFrom = gettvaluelocal(L, -1);
	if (pu->type == LANG_UPVAL_OPEN) {
		LangTValue *ptvTo = gettvalueglobal(L, pu->index);
		*ptvTo = *ptvFrom;
	} else {
		pu->tvalue = *ptvFrom;
	}
	list_pop(&L->stack, NULL);
}

void lang_tailcall(LangState *L, int nArg) {
	LangTValue *ptv = gettvaluelocal(L, -nArg - 1);
	if (ptv->type == LANG_TYPE_LFUNCTION) {
		lang_closeupvals(L, L->callInfo.stackFrame);
		int nRemove = L->stack.length - nArg - L->callInfo.stackFrame;
		list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
		LangFunction *plf = ptv->value.ptr;
		L->callInfo.pc = plf->ptr;
		L->callInfo.plfunction = plf;
	} else if (ptv->type == LANG_TYPE_CFUNCTION) {
		lang_closeupvals(L, L->callInfo.stackFrame);
		int nRemove = L->stack.length - nArg - L->callInfo.stackFrame;
		list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
		L->callInfo.stackFrame = L->stack.length - nArg;
		L->callInfo.plfunction = NULL;
		int nReturn = ((lang_cfunction)ptv->value.ptr)(L);
		int nReturnExpected = L->callInfo.numReturnExpected;
		if (nReturn < nReturnExpected) {
			for (int i = 0; i < nReturnExpected - nReturn; i++) {
				lang_pushnull(L);
			}
		} else if (nReturn > nReturnExpected) {
			list_popn(&L->stack, nReturn - nReturnExpected);
		}
		lang_closeupvals(L, L->callInfo.stackFrame);
		nRemove = L->stack.length - nReturnExpected - (L->callInfo.stackFrame - 1);
		list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
		list_pop(&L->prevCallInfos, &L->callInfo);
	} else {
		lang_errmsg(L, "bad invocation");
		return;
	}
}

void lang_tostring(LangState *L) {
	LangTValue *ptv = gettvaluelocal(L, -1);
	if (ptv->type == LANG_TYPE_STRING) {
		return;
	}
	char buf[LANG_MAX_TOSTRING_LEN];
	int len = lang_tostringbuf(ptv, buf);
	list_pop(&L->stack, NULL);
	lang_pushlstring(L, buf, len);
}

void lang_tonumber(LangState *L) {
	LangTValue *ptv = gettvaluelocal(L, -1);
	if (ptv->type == LANG_TYPE_NUMBER || ptv->type == LANG_TYPE_NULL) {
		return;
	}
	list_pop(&L->stack, NULL);
	switch (ptv->type) {
		case LANG_TYPE_STRING:
		{
			lang_number d;
			LangString *ps = ptv->value.ptr;
			if (lang_tonumberbuf(ps->data, ps->length, &d)) {
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
	LangTValue *ptv = gettvaluelocal(L, -1);
	lang_pop(L);
	switch (op) {
		case LANG_OP_NEG:
		{
			if (ptv->type != LANG_TYPE_NUMBER) {
				lang_errmsg(L, "expected a number");
				return;
			}
			lang_pushnumber(L, -ptv->value.number);
		} break;
		case LANG_OP_NOT:
		{
			if (ptv->type == LANG_TYPE_NUMBER) {
				lang_pushnumber(L, !ptv->value.number);
			} else if (ptv->type == LANG_TYPE_NULL) {
				lang_pushnumber(L, 1);
			} else {
				lang_pushnumber(L, 0);
			}
		} break;
		case LANG_OP_LEN:
		{
			if (ptv->type != LANG_TYPE_STRING) {
				lang_errmsg(L, "expected a string");
				return;
			}
			LangString *ps = ptv->value.ptr;
			lang_pushnumber(L, ps->length);
		} break;
		default:
			assert(0);
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
	LangTValue *ptv = gettvaluelocal(L, -1);
	return ptv->type == LANG_TYPE_NULL || (ptv->type == LANG_TYPE_NUMBER && ptv->value.number == 0);
}

void lang_registerfunc(LangState *L, const char *name, lang_cfunction func) {
	LangTValue tv;
	tv.type = LANG_TYPE_CFUNCTION;
	tv.value.ptr = func;
	stringhashtable_put(&L->importTable, name, &tv);
}

LangUserdata *lang_pushuserdata(LangState *L, void *src, size_t sz) {
	LangUserdata *po = newobject(L, sizeof(LangUserdata) + sz);
	memcpy(po->data, src, sz);

	LangTValue tv;
	tv.type = LANG_TYPE_USERDATA;
	tv.value.ptr = po;
	list_push(&L->stack, &tv);
	return po;
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

#define tostringbufliteral(l, out) snprintf((out), sizeof(l), ("" l))
int lang_tostringbuf(LangTValue *ptv, char *out) {
	switch (ptv->type) {
		case LANG_TYPE_NULL:
			return tostringbufliteral("null", out);
		case LANG_TYPE_NUMBER:
			return lang_tostringbufd(ptv->value.number, out);
		case LANG_TYPE_CFUNCTION:
		case LANG_TYPE_LFUNCTION:
			return snprintf(out, LANG_MAX_TOSTRING_LEN, "<function %p>", ptv->value.ptr);
		case LANG_TYPE_TABLE:
			return snprintf(out, LANG_MAX_TOSTRING_LEN, "<table %p>", ptv->value.ptr);
		default:
			return tostringbufliteral("<unknown>", out);
			break;
	}
}

int lang_tostringbufd(double d, char *out) {
	return snprintf(out, LANG_MAX_TOSTRING_LEN, "%.17g", d);
}

int lang_tostringbufi(int i, char *out) {
	return snprintf(out, LANG_MAX_TOSTRING_LEN, "%d", i);
}

LangState *lang_newstate() {
	LangState *L = malloc(sizeof(LangState));
	if (!L) {
		return NULL;
	}
	if (list_init(&L->stack, sizeof(LangTValue), LANG_STACK_BASE_SIZE)) {
		return NULL;
	}
	L->callInfo.stackFrame = 0;
	L->callInfo.plfunction = NULL;
	if (list_init(&L->prevCallInfos, sizeof(LangCallInfo), 1)) {
		return NULL;
	}
	if (stringhashtable_init(&L->importTable, sizeof(LangTValue), 2)) {
		return NULL;
	}
	L->gcLow = NULL;
	L->upvalOpen = NULL;
	L->upvalClosed = NULL;
	if (langC_init(&L->compilerState)) {
		return NULL;
	}
	L->errorCode = LANG_OK;
	L->msg = NULL;
	L->debug = no_cfunction;
	L->error = no_cfunction;
	return L;
}

void lang_atdebug(LangState *L, lang_cfunction debugf) {
	L->debug = debugf;
}

void lang_aterror(LangState *L, lang_cfunction errorf) {
	L->error = errorf;
}

#if _DEBUG
static void print(char* src, int length) {
	printf("%.*s", length, src);
}
#endif

void lang_load(LangState *L, const char *src) {
	L->msg = NULL;
	L->errorCode = LANG_OK;
	LangP_LexerState ls;
	List tokens = langP_tokenize(src, &ls);
	if (ls.msg) {
		list_free(&tokens);
		L->errorCode = LANG_ERROR_COMPILE;
		L->msg = ls.msg;
		L->error(L);
		return;
	}
	LangP_ParserState prs;
	LangP_AstNode *ast = langP_parse(src, &tokens, &prs);
	if (!ast) {
		list_free(&tokens);
		langP_free(ast);
		L->errorCode = LANG_ERROR_COMPILE;
		L->msg = prs.msg;
		L->error(L);
		return;
	}
	char *pbc;
	int len;
	int compilationResult = langC_compile(src, ast, &L->compilerState, &pbc, &len);
	list_free(&tokens);
	langP_free(ast);
	if (compilationResult) {
		L->errorCode = LANG_ERROR_COMPILE;
		L->msg = L->compilerState.msg;
		L->error(L);
		return;
	}
#if _DEBUG
	if (langV_print(print, pbc, len)) {
		return;
	}
#endif
	if (langV_exec(L, pbc, len)) {
		L->error(L);
	}
}

void lang_close(LangState *L) {
	stringhashtable_free(&L->importTable);
	list_free(&L->prevCallInfos);
	list_free(&L->stack);
	langC_free(&L->compilerState);
}