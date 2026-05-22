/*
* lang.h
*  Programming language API
*/

#ifndef LANG_H
#define LANG_H

#include "jaggedvector.h"

typedef struct {
	JaggedVector stack;
	const char *msg;
} LangState;

typedef struct {
	char* data;
	size_t length;
} LangString;

#define lang_errmsg(ps, _msg) (ps->msg = _msg)

int			lang_init_state		(LangState *ps);

#define LANG_OP_ADD		0
#define LANG_OP_SUB		1

void		lang_arith			(LangState *ps, char op);						// [-(2|1), +1]

#define lang_pushliteral(ps, l) (lang_pushlstring(ps, "" l, sizeof(l)))			// [-0, +1]

void		lang_pop			(LangState *ps, void *dst, size_t elemSize);	// [-1, +0]
void		lang_pushchar		(LangState *ps, char c);						// [-0, +1]
void		lang_pushlstring	(LangState *ps, const char *pstr, size_t len);	// [-0, +1]
char		lang_checkchar		(LangState *ps, int index);						// [-0, +0]
const char *lang_checklstring	(LangState *ps, int index, size_t *len);		// [-0, +0]

#endif // LANG_H