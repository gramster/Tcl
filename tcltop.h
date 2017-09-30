// First include file needed by all others

#ifndef __TCLBUG_H
#define __TCLBUG_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#if __MSDOS__
#include <dir.h>
#include <dos.h>
#include <io.h>
#include <fcntl.h>
#include <conio.h>
#include <process.h>
#else
#include <unistd.h>
#ifdef NO_DIRENT_H
#else
#include <dirent.h>
#endif
#endif

//--------------------------------------------------------
// Debuggery

void FailAssert(char *msg, char *fnm, int ln);

#ifdef TRON

#define assert(p) \
	((p) ? (void)0 : \
	       (void)(FailAssert(#p, __FILE__, __LINE__ ), ShowDbgStk() ))

#define DBG_STK		64

typedef struct
{
    char file[14];
    int line;
} dbg_stk_entry;

extern dbg_stk_entry dbg_stk[DBG_STK];
extern int dbg_stk_pos;

void ShowDbgStk(char *msg = NULL, void *arg = NULL);
void PushDbgStk(char *f, int l);
void PopDbgStk();

class Tracer
{
public:
    Tracer(char *f = "UNKNOWN", int l = 0)	{ PushDbgStk(f,l);	}
    ~Tracer()					{ PopDbgStk();		}
};

#define TraceThis()	Tracer tracer(__FILE__, __LINE__)

#else

#define TraceThis()
#define ShowDbgStk(msg, arg)
#define assert(p) \
	((p) ? (void)0 : (void) FailAssert(#p, __FILE__, __LINE__ ) )

#endif

//-----------------------------------------------------------

#define MAX_NEST	32

#define ETX	-1

// exceptions

#define OKAY	0
#define ERROR	1
#define RETURN	2
#define BREAK	3
#define CONT	4

// Sort keys

#define ASCII_KEY	0
#define INTEGER_KEY	1
#define REAL_KEY	2
#define COMMAND_KEY	3

#ifdef GW_DEBUG
#define ALLOC(len)	(char *)malloc(len)
#define FREE(t)		free(t)
#else
#define ALLOC(len)	new char [len]
#define FREE(t)		delete [] (char *)t
#endif

#endif

