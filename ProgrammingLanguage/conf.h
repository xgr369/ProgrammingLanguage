/*
* conf.h
* Configuration
*/

#ifndef CONF_H
#define CONF_H

#if defined(LANG_BUILD_AS_DLL)
#define LANG_API __declspec(dllexport)
#else
#define LANG_API
#endif

#endif // CONF_H