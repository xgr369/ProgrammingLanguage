#include <string.h>
#include "lang.h"

int lang_init_state(LangState *ps) {
	return jaggedvector_new(&ps->stack);
}

void lang_arith(LangState *ps, char op) {
	char a, b, result; // Eventually, this should be `int'

	lang_pop(&ps->stack, &a, 1);
	lang_pop(&ps->stack, &b, 1);
	switch (op) {
		case LANG_OP_ADD:
			result = a + b;
			break;
		case LANG_OP_SUB:
			result = a - b;
			break;
		default:
			lang_errmsg(ps, "unrecognized arithmetic operation");
			return;
	}
	lang_pushchar(ps, result);
}

void lang_pop(LangState *ps, void *dst, size_t elemSize) {
	jaggedvector_pop(&ps->stack, dst, elemSize);
}

void lang_pushchar(LangState *ps, char c) {
	jaggedvector_push(&ps->stack, &c, 1);
}

void lang_pushlstring(LangState *ps, const char *pstr, size_t len) {
	LangString ls;
	char *ptr = malloc(len);
	if (ptr == NULL) {
		lang_errmsg(ps, "malloc failed");
		return;
	}
	memcpy(ptr, pstr, len);
	ls.data = ptr;
	ls.length = len;
	jaggedvector_push(&ps->stack, &ls, sizeof(ls));
}

char lang_checkchar(LangState *ps, int index) {
	char c;
	if (index < 0) {
		jaggedvector_get(&ps->stack, ps->stack.table.length + index, &c, 1);
	} else {
		jaggedvector_get(&ps->stack, index, &c, 1);
	}
	return c;
}

const char *lang_checklstring(LangState *ps, int index, size_t *plen) {
	LangString ls;
	if (index < 0) {
		jaggedvector_get(&ps->stack, ps->stack.table.length + index, &ls, sizeof(ls));
	} else {
		jaggedvector_get(&ps->stack, index, &ls, sizeof(ls));
	}
	if (plen != NULL) {
		*plen = ls.length;
	}
	return ls.data;
}