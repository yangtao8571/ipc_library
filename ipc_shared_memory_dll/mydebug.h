#ifndef __MYDEBUG_H__
#define __MYDEBUG_H__

#ifdef MYDEBUG
#include <time.h>
#define mydebug(...) fprintf(stdout, "%ld\t%lu\t", time(NULL), GetCurrentThreadId()); fprintf(stdout, __VA_ARGS__);
#else
#define mydebug(...) ;
#endif

#endif __MYDEBUG_H__