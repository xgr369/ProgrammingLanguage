#include "core.h"
#include <string.h>
#include "compiler.h"
#include "vm.h"

#define LANG_MAX_TOSTRING_LEN 44
#define lang_inCframe(L) ((L)->callInfo.stackFrame && !(L)->callInfo.plfunction)

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

static void addgclow(LangState *L, LangObject *po) {
	po->gcNext = L->gcLow;
	L->gcLow = po;
	L->gcLowCount++;
}

static LangObject *newobject(LangState *L, size_t sz, char type) {
	LangObject *po = malloc(sz);
	if (!po) {
		lang_errmsg(L, "allocation failed");
		return NULL;
	}
	po->gcType = type;
	addgclow(L, po);
	return po;
}

static LangUpval *findupval(LangState *L, int index) {
	int idxGlobal = L->callInfo.stackFrame + index;
	LangUpval **ppu = &L->upvalOpen;
	while (*ppu != NULL && (*ppu)->value.index > idxGlobal) {
		ppu = &(*ppu)->gcNext;
	}
	if (*ppu != NULL && (*ppu)->value.index == idxGlobal) {
		return *ppu;
	}

	LangUpval *pu = malloc(sizeof(LangUpval));
	if (!pu) {
		lang_errmsg(L, "allocation failed");
		return NULL;
	}
	pu->status = LANG_UPVAL_OPEN;
	pu->value.index = idxGlobal;

	pu->gcType = LANG_GCTYPE_UPVAL;
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
		} else {
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
				case LANG_TYPE_FUNCTION:
					result.value.number = pa->value.ptr == pb->value.ptr;
					break;
				default:
					assert(0);
					return;
			}
		}
	} else if (op == LANG_OP_ADD) {
		int isString = pa->type == LANG_TYPE_STRING || pb->type == LANG_TYPE_STRING;
		if (isString) {
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
			LangString *ps = newobject(L, sizeof(LangString) + lena + lenb + 1, LANG_GCTYPE_STRING);
			if (!ps) {
				return;
			}
			ps->length = lena + lenb;
			memcpy(ps->data, ptra, lena);
			memcpy(ps->data + lena, ptrb, lenb);
			ps->data[lena + lenb] = '\0';
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
	langM_list_popn(&L->stack, 2);
	langM_list_push(&L->stack, &result);
}

void lang_call(LangState *L, int nArg, int nReturnExpected) {
	LangTValue *ptv = gettvaluelocal(L, -nArg - 1);
	if (ptv->type != LANG_TYPE_FUNCTION) {
		lang_errmsg(L, "value is not callable");
		return;
	}
	if (ptv->variant == LANG_VARIANT_LFUNC) {
		LangFunction *plf = ptv->value.ptr;
		if (nArg != plf->numParam) {
			lang_errmsg(L, "invalid number of arguments");
			return;
		}
		int fromC = lang_inCframe(L);
		L->callInfo.address = *L->paddress;
		langM_list_push(&L->prevCallInfos, &L->callInfo);
		L->callInfo.stackFrame = L->stack.length - nArg;
		L->callInfo.numReturnExpected = nReturnExpected;
		L->callInfo.plfunction = plf;
		if (fromC) {
			if (langV_exec(L, plf->ptr, L->prevCallInfos.length)) {
				L->error(L);
				return;
			}
		} else {
			*L->paddress = plf->ptr;
		}
	} else if (ptv->variant == LANG_VARIANT_CFUNC) {
		LangCallInfo callInfoPrev = L->callInfo;
		L->callInfo.stackFrame = L->stack.length - nArg;
		L->callInfo.plfunction = NULL;
		int nReturn = ((lang_cfunction)ptv->value.ptr)(L);
		if (nReturn < nReturnExpected) {
			for (int i = 0; i < nReturnExpected - nReturn; i++) {
				lang_pushnull(L);
			}
		} else if (nReturn > nReturnExpected) {
			langM_list_popn(&L->stack, nReturn - nReturnExpected);
		}
		lang_closeupvals(L, L->callInfo.stackFrame);
		int nRemove = L->stack.length - nReturnExpected - (L->callInfo.stackFrame - 1);
		langM_list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
		L->callInfo = callInfoPrev;
	}
}

// TODO: verify logic
static void gcmark(LangState *L, LangObject *po) {
	if (po->gcInfo == 1) {
		return;
	}
	po->gcInfo = 1;
	switch (po->gcType) {
		case LANG_GCTYPE_UPVAL: {
			LangUpval *pu = po;
			if (pu->status == LANG_UPVAL_CLOSED) {
				LangTValue *ptv = &pu->value.tv;
				if (ptv->type == LANG_TYPE_STRING || (ptv->type == LANG_TYPE_FUNCTION && ptv->variant == LANG_VARIANT_LFUNC) || ptv->type == LANG_TYPE_USERDATA) {
					gcmark(L, ptv->value.ptr);
				}
			}
		} break;
		case LANG_GCTYPE_LFUNC: {
			LangFunction *plf = po;
			for (int i = 0; i < plf->numUpval; i++) {
				gcmark(L, plf->upvalues[i]);
			}
		} break;
		case LANG_GCTYPE_UDATA: {
			LangUserdata *pu = po;
			LangObject **refs = pu->data + pu->dataSize;
			for (int i = 0; i < pu->numRefs; i++) {
				LangObject *ref = refs[i];
				gcmark(L, ref);
			}
		} break;
	}
}

#define LANG_GC_LOW_THRDECR 0.75f
#define LANG_GC_LOW_THRINCR 1.25f
void lang_collectgarbage(LangState *L) {
	LangObject *po = L->gcLow;
	while (po) {
		po->gcInfo = 0;
		po = po->gcNext;
	}
	for (int i = 0; i < L->stack.length; i++) {
		LangTValue *ptv = gettvalueglobal(L, i);
		if (ptv->type == LANG_TYPE_STRING || (ptv->type == LANG_TYPE_FUNCTION && ptv->variant == LANG_VARIANT_LFUNC) || ptv->type == LANG_TYPE_USERDATA) {
			gcmark(L, ptv->value.ptr);
		}
	}

	LangObject **ppo = &L->gcLow;
	while (*ppo) {
		po = *ppo;
		if (po->gcInfo == 0) {
			*ppo = po->gcNext;
			free(po);
			L->gcLowCount--;
		} else {
			ppo = &po->gcNext;
		}
	}
	if (L->gcLowCount < L->gcLowThreshold) {
		L->gcLowThreshold = L->gcLowCount * LANG_GC_LOW_THRDECR;
	} else {
		L->gcLowThreshold = L->gcLowCount * LANG_GC_LOW_THRINCR;
	}
}

void lang_export(LangState *L, const char *name) {
	LangTValue *ptv = gettvaluelocal(L, -1);
	if (langM_hash_put(&L->registry, name, ptv)) {
		lang_errmsg(L, "import value not found");
		return;
	}
	lang_pop(L);
}

void lang_getfield(LangState *L, const char *name, int len) {
	lang_errmsg(L, "fields have not been implemented yet");
}

void lang_getlocal(LangState *L, int index) {
	LangTValue *ptv = gettvaluelocal(L, index);
	langM_list_push(&L->stack, ptv);
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
	if (pu->status == LANG_UPVAL_OPEN) {
		ptv = gettvalueglobal(L, pu->value.index);
	} else {
		ptv = &pu->value.tv;
	}
	langM_list_push(&L->stack, ptv);
}

void lang_import(LangState *L, const char *name) {
	LangTValue *ptv = langM_hash_at(&L->registry, name);
	if (!ptv) {
		lang_errmsg(L, "import value not found");
		return;
	}
	langM_list_push(&L->stack, ptv);
}

void lang_pop(LangState *L) {
	langM_list_pop(&L->stack, NULL);
}

void lang_popn(LangState *L, int n) {
	langM_list_popn(&L->stack, n);
}

void lang_pushlstring(LangState *L, const char *str, int len) {
	LangString *ps = newobject(L, sizeof(LangString) + len + 1, LANG_GCTYPE_STRING);
	if (!ps) {
		return;
	}
	ps->length = len;
	memcpy(ps->data, str, len);
	ps->data[len] = '\0';

	LangTValue tv;
	tv.type = LANG_TYPE_STRING;
	tv.value.ptr = ps;
	langM_list_push(&L->stack, &tv);
}

void lang_pushnull(LangState *L) {
	LangTValue tv;
	tv.type = LANG_TYPE_NULL;
	langM_list_push(&L->stack, &tv);
}

void lang_pushnumber(LangState *L, lang_number value) {
	LangTValue tv;
	tv.type = LANG_TYPE_NUMBER;
	tv.value.number = value;
	langM_list_push(&L->stack, &tv);
}

void lang_pushrange(LangState *L, int start, int end) {
	LangTValue tv;
	tv.type = LANG_TYPE_RANGE;
	tv.value.range.start = start;
	tv.value.range.end = end;
	langM_list_push(&L->stack, &tv);
}

void lang_pushthis(LangState *L) {
	if (L->callInfo.plfunction == NULL) {
		lang_errmsg(L, "this not in function");
		return;
	}
	LangTValue *ptv = gettvaluethis(L);
	langM_list_push(&L->stack, ptv);
}

void lang_removen(LangState *L, int index, int n) {
	langM_list_removen(&L->stack, L->callInfo.stackFrame + index, n);
}

void lang_setfield(LangState *L, const char *name, int len) {
	lang_errmsg(L, "fields have not been implemented yet");
}

void lang_setlocal(LangState *L, int index) {
	LangTValue *ptv = gettvaluelocal(L, -1);
	langM_list_set(&L->stack, L->callInfo.stackFrame + index, ptv);
	langM_list_pop(&L->stack, NULL);
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
	if (pu->status == LANG_UPVAL_OPEN) {
		LangTValue *ptvTo = gettvalueglobal(L, pu->value.index);
		*ptvTo = *ptvFrom;
	} else {
		pu->value.tv = *ptvFrom;
	}
	langM_list_pop(&L->stack, NULL);
}

void lang_tostring(LangState *L) {
	LangTValue *ptv = gettvaluelocal(L, -1);
	if (ptv->type == LANG_TYPE_STRING) {
		return;
	}
	char buf[LANG_MAX_TOSTRING_LEN];
	int len = lang_tostringbuf(ptv, buf);
	langM_list_pop(&L->stack, NULL);
	lang_pushlstring(L, buf, len);
}

void lang_tonumber(LangState *L) {
	LangTValue *ptv = gettvaluelocal(L, -1);
	if (ptv->type == LANG_TYPE_NUMBER || ptv->type == LANG_TYPE_NULL) {
		return;
	}
	langM_list_pop(&L->stack, NULL);
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
	LangTValue result;
	switch (op) {
		case LANG_OP_NEG:
		{
			if (ptv->type != LANG_TYPE_NUMBER) {
				lang_errmsg(L, "expected a number");
				return;
			}
			result.type = LANG_TYPE_NUMBER;
			result.value.number = -ptv->value.number;
		} break;
		case LANG_OP_NOT:
		{
			result.type = LANG_TYPE_NUMBER;
			if (ptv->type == LANG_TYPE_NUMBER) {
				result.value.number = !ptv->value.number;
			} else if (ptv->type == LANG_TYPE_NULL) {
				result.value.number = 1;
			} else {
				result.value.number = 0;
			}
		} break;
		case LANG_OP_LEN:
		{
			if (ptv->type != LANG_TYPE_STRING) {
				lang_errmsg(L, "expected a string");
				return;
			}
			LangString *ps = ptv->value.ptr;
			result.type = LANG_TYPE_NUMBER;
			result.value.number = ps->length;
		} break;
		default:
			assert(0);
			return;
	}
	langM_list_pop(&L->stack, NULL);
	langM_list_push(&L->stack, &result);
}

void lang_closeupvals(LangState *L, int index) {
	LangUpval **ppu = &L->upvalOpen;
	while (*ppu != NULL && (*ppu)->value.index >= index) {
		LangUpval *pu = *ppu;
		LangUpval *puNext = pu->gcNext;

		LangTValue *ptv = gettvalueglobal(L, pu->value.index);
		pu->status = LANG_UPVAL_CLOSED;
		pu->value.tv = *ptv;
		
		addgclow(L, pu);
		*ppu = puNext;
	}
	L->upvalOpen = *ppu;
}

void lang_pushlfunction(LangState *L, const char *src, int nParam, int nUpval, const char *upvals) {
	LangFunction *plf = newobject(L, sizeof(LangFunction) + sizeof(LangUpval) * nUpval, LANG_GCTYPE_LFUNC);
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
	tv.type = LANG_TYPE_FUNCTION;
	tv.variant = LANG_VARIANT_LFUNC;
	tv.value.ptr = plf;
	langM_list_push(&L->stack, &tv);
}

int lang_iszero(LangState *L) {
	LangTValue *ptv = gettvaluelocal(L, -1);
	return ptv->type == LANG_TYPE_NULL || (ptv->type == LANG_TYPE_NUMBER && ptv->value.number == 0);
}

#include <Windows.h>
void lang_pushuserdata(LangState *L, int dataSize, int nRefs, void *srcData, LangObject **srcRef) {
	LangUserdata *pu = newobject(L, sizeof(LangUserdata) + dataSize + nRefs * sizeof(LangObject *), LANG_GCTYPE_UDATA);
	pu->dataSize = dataSize;
	pu->numRefs = nRefs;
	memcpy(pu->data, srcData, dataSize);
	memcpy(pu->data + dataSize, srcRef, nRefs * sizeof(LangObject *));

	LangTValue tv;
	tv.type = LANG_TYPE_USERDATA;
	tv.value.ptr = pu;
	langM_list_push(&L->stack, &tv);
}

void lang_registerfunc(LangState *L, const char *name, lang_cfunction func) {
	LangTValue tv;
	tv.type = LANG_TYPE_FUNCTION;
	tv.variant = LANG_VARIANT_CFUNC;
	tv.value.ptr = func;
	langM_hash_put(&L->registry, name, &tv);
}

void lang_return(LangState *L, int nReturn, int baseFrame) {
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
		langM_list_popn(&L->stack, nReturn - nReturnExpected);
	}
	lang_closeupvals(L, L->callInfo.stackFrame);
	int nRemove = L->stack.length - nReturnExpected - (L->callInfo.stackFrame - 1);
	langM_list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
	langM_list_pop(&L->prevCallInfos, &L->callInfo);
	*L->paddress = L->callInfo.address;
	if (L->prevCallInfos.length < baseFrame) {
		L->msgCode = LANG_EXIT;
	}
}

void lang_tailcall(LangState *L, int nArg) {
	LangTValue *ptv = gettvaluelocal(L, -nArg - 1);
	if (ptv->type != LANG_TYPE_FUNCTION) {
		lang_errmsg(L, "value is not callable");
		return;
	}
	if (ptv->variant == LANG_VARIANT_LFUNC) {
		lang_closeupvals(L, L->callInfo.stackFrame);
		int nRemove = L->stack.length - nArg - L->callInfo.stackFrame;
		langM_list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
		LangFunction *plf = ptv->value.ptr;
		L->callInfo.plfunction = plf;
		*L->paddress = plf->ptr;
	} else if (ptv->variant == LANG_VARIANT_CFUNC) {
		lang_closeupvals(L, L->callInfo.stackFrame);
		int nRemove = L->stack.length - nArg - L->callInfo.stackFrame;
		langM_list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);

		ptv = gettvaluelocal(L, -nArg - 1);
		L->callInfo.stackFrame = L->stack.length - nArg;
		L->callInfo.plfunction = NULL;
		int nReturn = ((lang_cfunction)ptv->value.ptr)(L);
		int nReturnExpected = L->callInfo.numReturnExpected;
		if (nReturn < nReturnExpected) {
			for (int i = 0; i < nReturnExpected - nReturn; i++) {
				lang_pushnull(L);
			}
		} else if (nReturn > nReturnExpected) {
			langM_list_popn(&L->stack, nReturn - nReturnExpected);
		}
		lang_closeupvals(L, L->callInfo.stackFrame);
		nRemove = L->stack.length - nReturnExpected - (L->callInfo.stackFrame - 1);
		langM_list_removen(&L->stack, L->callInfo.stackFrame - 1, nRemove);
		langM_list_pop(&L->prevCallInfos, &L->callInfo);
		*L->paddress = L->callInfo.address;
	}
}

LangTValue *lang_gettvalueglobal(LangState *L, int index) {
	return gettvalueglobal(L, index);
}

LangTValue *lang_gettvaluelocal(LangState *L, int index) {
	return gettvaluelocal(L, index);
}

void lang_pushtvalue(LangState *L, LangTValue *ptv) {
	langM_list_push(&L->stack, ptv);
}

int lang_tonumberbuf(const char *str, int len, lang_number *out) {
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
		case LANG_TYPE_FUNCTION:
			return snprintf(out, LANG_MAX_TOSTRING_LEN, "<function %p>", ptv->value.ptr);
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
	if (langM_list_init(&L->stack, sizeof(LangTValue), LANG_STACK_BASE_SIZE)) {
		return NULL;
	}
	if (langM_list_init(&L->prevCallInfos, sizeof(LangCallInfo), 1)) {
		return NULL;
	}
	if (langM_hash_init(&L->registry, sizeof(LangTValue), 2)) {
		return NULL;
	}
	L->gcLow = NULL;
	L->upvalOpen = NULL;
	L->callInfo.stackFrame = 0;
	// ignore L->callInfo.numReturnExpected
	// ignore L->callInfo.address
	L->callInfo.plfunction = NULL;
	L->src = NULL;
	L->srcLen = 0;
	// ignore L->paddress
	L->gcLowCount = 0;
	L->gcLowThreshold = 0;
	L->msgCode = LANG_OK;
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

int lang_clear(LangState *L) {
	lang_closeupvals(L, 0);
	if (langM_list_clear(&L->stack, LANG_STACK_BASE_SIZE)) {
		lang_errmsg(L, "internal error");
		return 1;
	}
	if (langM_list_init(&L->prevCallInfos, sizeof(LangCallInfo), 1)) {
		lang_errmsg(L, "internal error");
		return 1;
	}
	L->callInfo.stackFrame = 0;
	L->callInfo.plfunction = NULL;
	L->msgCode = LANG_OK;
	L->msg = NULL;
	return 0;
}

void lang_close(LangState *L) {
	langM_list_free(&L->stack);
	langM_list_free(&L->prevCallInfos);
	langM_hash_free(&L->registry);
	LangObject *po = L->gcLow;
	LangObject *poNext;
	while (po) {
		poNext = po->gcNext;
		free(po);
		po = poNext;
	}
	po = L->upvalOpen;
	while (po) {
		poNext = po->gcNext;
		free(po);
		po = poNext;
	}
}

void lang_load(LangState *L, const char *src) {
	L->msg = NULL;
	L->msgCode = LANG_OK;

	LangP_LexerState ls;
	LangM_List tokens = langP_tokenize(src, &ls);
	if (ls.msg) {
		langM_list_free(&tokens);

		L->msgCode = LANG_ERR_COMPILE;
		L->msg = ls.msg;
		L->error(L);
		return;
	}

	LangP_ParserState ps;
	LangP_AstNode *ast = langP_parse(src, &tokens, &ps);
	if (!ast) {
		langM_list_free(&tokens);
		langP_free(ast);

		L->msgCode = LANG_ERR_COMPILE;
		L->msg = ps.msg;
		L->error(L);
		return;
	}

	LangC_CompilerState cs;
	int result = langC_compile(src, ast, &cs, &L->src, &L->srcLen);
	langM_list_free(&tokens);
	langP_free(ast);
	langC_free(&cs);
	if (result) {
		L->msgCode = LANG_ERR_COMPILE;
		L->msg = cs.msg;
		L->error(L);
		return;
	}

#if _DEBUG
	if (langV_print(L->src, L->srcLen)) {
		return;
	}
#endif

	if (langV_exec(L, L->src, 0)) {
		L->error(L);
	}
	free(L->src);
}