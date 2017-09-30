#ifndef __TCL_H
#define __TCL_H

#include "tcltop.h"
#include "vars.h"
#include "lists.h"
#include "awklib.h"

#if __MSDOS__
#define PATH_SEP	'\\'
#else
#define PATH_SEP	'/'
#endif

// Prototypes from parse.cpp

extern int ch;

void nextchar(InputString *ip);
int ParseString(InputString *ip, String &toke, int end);
int ParseVariable(InputString *ip, String &toke, int expand=1);
int ParseImmed(InputString *ip, String &toke);
int ParseBlock(InputString *ip, String &toke);
int GetWord(InputString *ip, String &result);
int ReadStatement(InputString *ip, StringGroup &args, String &result);

// Prototypes from lists.cpp

int SplitList(char *list, StringGroup &elts, int lim);
int SplitList(String &list, StringGroup &elts, int lim);
void AddListElt(String &res, char *elt, char *sep = " ", int escape = 0);
void AddListElt(String &res, String &elt, char *sep = " ", int escape = 0);
int FormatString(StringGroup &args, String &res, int first = 0);
int ScanString(String &fmtstr, String &input, StringGroup &res);

// Prototype from expr.cpp

extern char *compute(char *str, int *rtn);

// Prototype from tcl.cpp

char *SplitVar(String v, String* &base, String* &idx);
int FindString(char *nm, char *tbl[], int tblsz);
int Interpret(String *input, String &result);

extern FILE *debug, *input;
extern String interpResult;

// Prototypes from pattern.cpp

char *SetRegexpPattern(char *pat);
int RegexpMatch(char *str);
char *SetGlobPattern(char *pat);
int GlobMatch(char *str);

// from file.cpp

void SaveStatResults(struct stat *st, char *aname);

#endif

