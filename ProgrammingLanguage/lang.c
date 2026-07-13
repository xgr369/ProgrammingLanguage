#include <string.h>
#include "lang.h"

inline void lang_gettvalue(LangState *ps, int index, LangTValue *dst) {
	if (index < 0) {
		vector_get(&ps->stack, ps->stack.length + index, dst);
	} else {
		vector_get(&ps->stack, index, dst);
	}
}

int lang_init_state(LangState *ps) {
	if (vector_new(&ps->stack, sizeof(LangTValue), LANG_STACK_BASE_SIZE)) {
		return 1;
	}
	if (stringhashtable_new(&ps->externValueTable, sizeof(LangTValue), 2)) {
		return 1;
	}
	ps->msg = NULL;
	ps->callInfo.stackFrame = 0;
	if (vector_new(&ps->prevCallInfos, sizeof(LangCallInfo), 1)) {
		return 1;
	}
	return 0;
}

// Todo: rethink this logic
void lang_binaryop(LangState *ps, char op) {
	LangTValue a, b;
	lang_gettvalue(ps, -1, &b);
	lang_gettvalue(ps, -2, &a);
	lang_popn(ps, 2);
	if (a.type == LANG_TYPE_NUMBER && b.type == LANG_TYPE_NUMBER) {
		switch (op) {
			case LANG_OP_ADD:
				lang_pushnumber(ps, a.value.number + b.value.number);
				break;
			case LANG_OP_SUB:
				lang_pushnumber(ps, a.value.number - b.value.number);
				break;
			case LANG_OP_MUL:
				lang_pushnumber(ps, a.value.number * b.value.number);
				break;
			case LANG_OP_DIV:
				if (b.value.number == 0) {
					lang_errmsg(ps, "division by zero");
					return;
				}
				lang_pushnumber(ps, a.value.number / b.value.number);
				break;
			case LANG_OP_EQ:
				lang_pushnumber(ps, a.value.number == b.value.number);
				break;
			case LANG_OP_LT:
				lang_pushnumber(ps, a.value.number < b.value.number);
				break;
			case LANG_OP_GT:
				lang_pushnumber(ps, a.value.number > b.value.number);
				break;
			default:
				lang_errmsg(ps, "unrecognized binary operation on type number");
				return;
		}
	} else if (a.type == LANG_TYPE_STRING && b.type == LANG_TYPE_STRING) {
		switch (op) {
			case LANG_OP_EQ:
			{
				LangString *plsa = a.value.ptr;
				LangString *plsb = b.value.ptr;
				lang_pushnumber(ps, plsa->length == plsb->length && strncmp(&plsa->data, &plsb->data, plsa->length) == 0);
				break;
			}
			default:
				lang_errmsg(ps, "unrecognized binary operation on type string");
				return;
		}
	} else {
		lang_errmsg(ps, "binary operation on unrecognized type(s)");
		return;
	}
}

void lang_copy(LangState *ps, int indexFrom, int indexTo) {
	LangTValue ltv;
	lang_gettvalue(ps, indexFrom, &ltv);
	vector_set(&ps->stack, indexTo, &ltv);
}

void lang_endcall(LangState *ps, int nReturn) {
	if (nReturn < ps->callInfo.nReturnExpected) {
		for (int i = nReturn; i < nReturn; i++) {
			lang_pushnil(ps);
		}
	} else if (nReturn > ps->callInfo.nReturnExpected) {
		vector_popn(&ps->stack, nReturn - ps->callInfo.nReturnExpected);
	}
	int nRemove = ps->stack.length - ps->callInfo.nReturnExpected - (ps->callInfo.stackFrame - 1);
	vector_removen(&ps->stack, ps->callInfo.stackFrame - 1, nRemove);
	vector_pop(&ps->prevCallInfos, &ps->callInfo);
}

void lang_loadexternvalue(LangState *ps, const char *name) {
	LangTValue ltv;
	if (stringhashtable_get(&ps->externValueTable, name, &ltv)) {
		lang_errmsg(ps, "extern value not found");
		return;
	}
	vector_push(&ps->stack, &ltv);
}

void lang_pop(LangState *ps) {
	vector_pop(&ps->stack, NULL);
}

void lang_popn(LangState *ps, int n) {
	vector_popn(&ps->stack, n);
}

void lang_precall(LangState *ps, int nArg, int nReturn) {
	vector_push(&ps->prevCallInfos, &ps->callInfo);
	LangTValue ltv;
	lang_gettvalue(ps, -nArg - 1, &ltv);
	if (ltv.type != LANG_TYPE_FUNCTION) {
		lang_errmsg(ps, "calling a non-function");
		return;
	}
	ps->callInfo.pc = ltv.value.ptr;
	ps->callInfo.stackFrame = ps->stack.length - nArg;
	ps->callInfo.nReturnExpected = nReturn;
}

void lang_pushnil(LangState *ps) {
	LangTValue ltv;
	ltv.type = LANG_TYPE_NIL;
	vector_push(&ps->stack, &ltv);
}

void lang_pushnumber(LangState *ps, lang_number value) {
	LangTValue ltv;
	ltv.type = LANG_TYPE_NUMBER;
	ltv.value.number = value;
	vector_push(&ps->stack, &ltv);
}

void lang_pushlfunc(LangState *ps, const void *src) {
	LangTValue ltv;
	ltv.type = LANG_TYPE_FUNCTION;
	ltv.value.ptr = src;
	vector_push(&ps->stack, &ltv);
}

void lang_pushlstring(LangState *ps, const char *str, int len) {
	LangString *pls = malloc(sizeof(LangString) + len); // todo: perhaps organize this code (LangString allocation) elsewhere
	if (!pls) {
		lang_errmsg(ps, "malloc failed");
		return;
	}
	pls->length = len;
	memcpy(pls->data, str, len);
	LangTValue ltv;
	ltv.type = LANG_TYPE_STRING;
	ltv.value.ptr = pls;
	vector_push(&ps->stack, &ltv);
}

void lang_pushvalue(LangState *ps, int index) {
	LangTValue ltv;
	lang_gettvalue(ps, ps->callInfo.stackFrame + index, &ltv);
	vector_push(&ps->stack, &ltv);
}

void lang_removen(LangState *ps, int index, int n) {
	vector_removen(&ps->stack, ps->callInfo.stackFrame + index, n);
}

void lang_replace(LangState *ps, int index) {
	LangTValue ltv;
	lang_gettvalue(ps, -1, &ltv);
	vector_set(&ps->stack, ps->callInfo.stackFrame + index, &ltv);
	lang_pop(ps);
}

int lang_iszero(LangState *ps) {
	LangTValue *pltv = &ps->stack.data[sizeof(LangTValue) * (ps->stack.length - 1)];
	return pltv->type == LANG_TYPE_NIL || (pltv->type == LANG_TYPE_NUMBER && pltv->value.number == 0);
}


lang_number lang_tonumber(LangState *ps) {
	LangTValue *pltv = &ps->stack.data[sizeof(LangTValue) * (ps->stack.length - 1)];
	if (pltv->type == LANG_TYPE_NUMBER) {
		return pltv->value.number;
	}
	return 0;
}



/*void lang_storeexternvalue(LangState *ps, const char *str, lang_cfunction func) {
	LangTValue ltv;
	ltv.type = LANG_TYPE_FUNCTION;
	ltv.value.cfunction = func;
	stringhashtable_put(&ps->externValueTable, str, &ltv);
}*/

/*
// REWRITE with lang_gettvalue
const char *lang_checklstring(LangState *ps, int index, size_t *dstLen) {
	LangTValue ltv;
	if (index < 0) {
		vector_get(&ps->stack, ps->stack.length + index, &ltv);
	} else {
		vector_get(&ps->stack, index, &ltv);
	}
	LangString *pls = (LangString *)ltv.value.ptr;
	if (dstLen) {
		*dstLen = pls->length;
	}
	return pls->data;
}*/


/*#include "vm.h"
void lang_call(LangState *ps, int nArg, int nReturn) {
	// wip
	// calls a function, with func, ...args [nArg] atop stack.
	// erases those values, leaving only ...returns [nReturn] atop.
	LangTValue ltv;
	lang_gettvalue(ps, -nArg - 1, &ltv);
	if (ltv.type != LANG_TYPE_FUNCTION) {
		lang_errmsg(ps, "calling a non-function");
		return;
	}
	char *ptr = ltv.value.ptr;
	int callerStackFrame = ps->callInfo.stackFrame;
	int calleeStackFrame = ps->stack.length - nArg;
	ps->callInfo.stackFrame = calleeStackFrame;
	langV_dbg(ptr, 100);
	if (langV_exec(ps, ptr, 1000)) { // test
		return;
	}
	ps->callInfo.stackFrame = callerStackFrame;
	int nReturnActual = ps->numReturn;
	if (nReturnActual < nReturn) {
		for (int i = nReturnActual; i < nReturn; i++) {
			lang_pushnil(ps);
		}
	} else if (nReturnActual > nReturn) {
		vector_popn(&ps->stack, nReturnActual - nReturn);
	}
	int nRemove = ps->stack.length - nReturn - (calleeStackFrame - 1);
	vector_removen(&ps->stack, calleeStackFrame - 1, nRemove);
}*/