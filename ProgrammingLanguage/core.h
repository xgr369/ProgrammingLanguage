/*
* core.h
*  Core
*/

#ifndef CORE_H
#define CORE_H

#include "conf.h"
#include "lang.h"

#define LANG_STACK_BASE_SIZE 8

#define LANG_GCTYPE_STRING 0
#define LANG_GCTYPE_UPVAL 1
#define LANG_GCTYPE_LFUNC 2
#define LANG_GCTYPE_UDATA 3
#define LANG_UPVAL_OPEN		0
#define LANG_UPVAL_CLOSED	1
#define LANG_UPVAL_DIRECT	0
#define LANG_UPVAL_INDIRECT	1

void		lang_closeupvals		(LangState *L, int index);					// [-0, +0]
void		lang_pushlfunction		(LangState *L, const char *src, int nParam,
										int nUpval, const char *upvals);		// [-0, +1]
void		lang_return 			(LangState *L, int nReturn, int frame);		// [-?, +0]
void		lang_tailcall			(LangState *L, int nArg);					// [-?, +0]

#endif // CORE_H