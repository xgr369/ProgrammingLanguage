/*
* conf.h
* Configuration
*/

#ifndef CONF_H
#define CONF_H

#include <assert.h>

#if defined(LANG_BUILD_AS_DLL)
#define LANG_API __declspec(dllexport)
#else
#define LANG_API
#endif

#endif // CONF_H