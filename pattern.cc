/****************************************************

	GC REGULAR EXPRESSION MATCHING

 Supports both BSD and System V regular expressions,
 as well as DOS file patterns. The latter are explicitly
 coded here; the others rely on library routines.

 Entry points to the code in this file are:

	char *compileRE(char *RE)
	int  matchRE(char *string)

 The former is used to prepare the regular expression
 while the latter returns a non-zero value if the
 given string matches the last regular expression set
 up with compileRE.

 (c) 1995 by Graham Wheeler

*****************************************************/

#include "tcl.h"
#pragma hdrstop

//#include "awklib.c"

/*****************************************************
	SYSTEM V SPECIFIC REGEXPS
******************************************************/

#ifdef USE_SYSV_REGEXP

static char *REmsg, *REsrc, REbuf[512];

#define INIT		register char *sp=REsrc;
#define GETC()		(*sp++)
#define UNGETC(c)	(sp--)
#define RETURN(c)	return c;
#define ERROR		REerr
#define PEEKC()		(*sp)

static void REerr(int c)
{
    switch(c) {
    case 11: REmsg = "range endpoint too large"; break;
    case 16: REmsg = "bad number"; break;
    case 25: REmsg = "\\ digit out of range"; break;
    case 36: REmsg = "illegal or missing delimeter"; break;
    case 41: REmsg = "no remembered search string"; break;
    case 42: REmsg = "\\( \\) imbalance"; break;
    case 43: REmsg = "too many \\('s"; break;
    case 44: REmsg = "more than two numbers in \\{ \\}"; break;
    case 45: REmsg = "} expected after \\"; break;
    case 46: REmsg = "first number exceeds second in \\{ \\}"; break;
    case 49: REmsg = "[ ] imbalance"; break;
    case 50: REmsg = "regular expression overflow"; break;
    default: REmsg = "unknown error"; break;
    }
}

#include <regexp.h>

char *SetRegexpPattern(char *RE)
{
    REmsg = NULL;
    REsrc = RE;
    (void)compile((char *)NULL,REbuf,REbuf+512,'\0');
    return REmsg;
}

int  RegexpMatch(char *string) 
{
    return !(step(string,REbuf)); // check!!
}

#endif

/*****************************************************
	BSD SPECIFIC REGEXPS
******************************************************/

#ifdef USE_BSD_REGEXP

char *SetRegexpPattern(char *RE)
{
    return (char *)re_comp(RE);
}

int  RegexpMatch(char *string)
{
    return !(re_exec(string)==1); // check!!
}

#endif

static char regexp[512];

char *SetRegexpPattern(char *RE)
{
    return (makepat(RE, regexp) == NULL) ? "Error in regexp" : NULL;
}

int  RegexpMatch(char *string)
{
    return (re_match (string, regexp) != NULL);
}

static String globexp;

int GlobMatch(char *str)
{
    char *pat = (char *)globexp;
#if __MSDOS__
    String s(str);
    str = (char *)s;
    strupr(str);
#endif
    int mismatch = 0;
    if (pat==NULL)
    	return (*str==0);
    while (*str)
    {
	char *oldpat = pat;
    	if (*pat == '\\')
	{
	    pat++;
    	    if (*pat!='?' && *pat != *str)
		goto fail;
	}
    	else if (*pat == '[')
	{
	    int inv = 0;
	    pat++;
	    if (*pat=='^') { pat++; inv = 1; }
	    while (*pat != ']')
	    {
		int first = *pat++, last;
		if (*pat == '-')
		{
		    pat++;
		    last = *pat++;
		}
		else last = first;
		if (*str >= first && *str<=last)
		{
		    if (!inv) break; // match
		}
		else if (inv) break; // match
	    }
	    if (*pat==']')
		goto fail;
	    while (*pat != ']')
	    {
		if (*pat=='\\') pat++;
		pat++;
	    }
	}
    	else if (*pat == '*')
	{
	    mismatch = 1;
	    pat++;
	    continue;
	}
    	else if (*pat!='?' && *pat != *str)
	    goto fail;
	mismatch = 0;
	pat++;
	str++;
	continue;
    fail:
	if (mismatch)
	{
	   pat = oldpat;
	   str++;
	}
	else return 0;
    }
    return (*pat==0); // return 0 if end of pattern
}

char *SetGlobPattern(char *RE)
{
    int inrange = 0;
    char *p;
    globexp = RE;
#if __MSDOS__
    strupr(p =(char *)globexp);
#endif
    while (*p)
    {
	if (*p == '\\')
	{
	    if (*++p == 0)
		goto badexp;
	}	
	else if (inrange && *p == '-')
	{
	    p++;
	    if (*p == ']' || *p==0) goto badexp;
	}
	else if (*p == '[')
	    inrange = 1;
	else if (*p == ']' && inrange)
	    inrange = 0;
	p++;
    }
    if (inrange == 0)
	return NULL;
badexp:
    return "Error in glob pattern";
}

