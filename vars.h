#ifndef __VARS_H
#define __VARS_H

#include "gstring.h"

#define NORMVAR		0
#define ALIASVAR	1
#define PROFVAR		2
#define FILEVAR		3
#define TRACEVAR	4
#define ARRAYVAR	5
#define PROCVAR		6
#define SEARCHVAR	7

class VariableTree;

class Variable
{
    String name;
    short type;
    friend class TraceableVariable;
public:
    Variable(int typ, char *nm = NULL)
    {
	type = typ;
    	name = nm ? nm : "";
    }
    virtual ~Variable() {};
    String &Name()
    {
    	return name;
    }
    // Shouldn't allow renaming; screws up binary search trees
    // This is only for the sentinel in such a tree
    void Rename(char *nm)
    {
    	name = nm;
    }
    int IsAlias()	{ return type == ALIASVAR; }
    int IsArray()	{ return type == ARRAYVAR; }
    int IsTrace()	{ return type == TRACEVAR; }
    int IsNormal()	{ return type == NORMVAR; }
    int IsTraceable()	{ return IsNormal() || IsArray(); }
};

#define TRACE_SET	1
#define TRACE_GET	2
#define TRACE_UNSET	4
#define TRACE_BUSY	8

class Tracer
{
    unsigned short flags;
    String handler;
    Tracer *next;
public:
    Tracer(unsigned f, char *h, Tracer *nxt = NULL)
    {
	flags = f;
	handler = h;
	next = nxt;
    }
    ~Tracer();
    void Next(Tracer *nxt) { next = nxt; }
    Tracer *Next() { return next; }
    char *Handler() { return (char *)handler; }
    unsigned short Flags() { return flags; }
    char *Ops();
    void TraceInfo(String &res);
    int Handle(unsigned evt, char *name, char *index = NULL);
};

typedef int (*Walker)(Variable *v, void *arg);

class VarTreeEntry
{
    Variable		*v;
    VarTreeEntry	*l, *r;
    friend class VariableTree;
    friend class SearchVariable;
public:
    VarTreeEntry(Variable *var,
    	VarTreeEntry *left = NULL, VarTreeEntry *right = NULL)
    {
	TraceThis();
        v = var;
        l = left;
        r = right;
    }
    ~VarTreeEntry()
    {
	TraceThis();
    	if (v) delete v;
    }
    String &Name()
    {
	TraceThis();
        return v->Name();
    }
    friend int WalkNode(VarTreeEntry *p, VarTreeEntry *s, Walker w, void *arg);
};

class VariableTree
{
    VarTreeEntry *root, *sentinel;
    int vcnt;
    void Trash(VarTreeEntry *p);
    friend class SearchVariable;
public:
    int Count() { return vcnt; }
    VariableTree();
    ~VariableTree();
    Variable *Fetch(char *name);
    Variable *Search(char *name);
    int Delete(char *name, char *array = NULL);
    void AddVar(Variable *v);
    int Walk(Walker w, void *arg = NULL);
};

class TraceableVariable : public Variable
{
    Tracer *t;
public:
    TraceableVariable(int typ, char *name)
	: Variable(typ, name), t(NULL)
    { }
    virtual ~TraceableVariable();
    Tracer *GetTrace()
	{ return t; }
    void Trace(unsigned f, char *h)
        { t = new Tracer(f, h, t); }
    void Untrace(unsigned f, char *h);
    void TraceInfo(String &res);
};

class ScalarVariable : public TraceableVariable
{
    String value;
public:
    ScalarVariable(char *name, char *val = NULL)
	: TraceableVariable(NORMVAR, name), value(val)
    	{ }
    void Set(char *val)
    	{ value = val; }
    char *GetContents()
    	{ return (char *)value; }
};

class ArrayVariable : public TraceableVariable
{
    VariableTree	elements;
public:
    ArrayVariable(char *name)
	: TraceableVariable(ARRAYVAR, name)
    	{ }
    VariableTree *Elements()
    	{ return &elements; }
};

class AliasVariable : public Variable
{
    Variable *av;	// real containing array variable if elt
    Variable *rv;	// real variable
    int lvl;
public:
    AliasVariable(char *name, Variable *real, int l, Variable *a = NULL)
	: Variable(ALIASVAR, name), rv(real), lvl(l), av(a)
    { }
    Variable *Real()
	{ return rv; }
    int Level()
	{ return lvl<0?0:lvl; }
    int IsGlobal()
	{ return (lvl<0); }
    ArrayVariable *Array()
	{ return (ArrayVariable *)av; }
};

class FileVariable : public Variable
{
    FILE *fp;	// real variable
public:
    FileVariable(char *name, FILE *fp_in)
	: Variable(FILEVAR, name), fp(fp_in)
    	{ }
    FILE *GetFP()
    	{ return fp; }
};

class SearchVariable : public Variable
{
    VariableTree *t;
    VarTreeEntry *now;
public:
    SearchVariable(char *name, VariableTree *t_in)
	: Variable(SEARCHVAR, name), t(t_in), now(t_in->root->r)
    	{ }
    int AnyMore();
    char *Next();
};

class ProcVariable : public Variable
{
    int internal;
public:
    ProcVariable(char *name, int is_int)
	: Variable(PROCVAR, name), internal(is_int)
    	{ }
    int IsInternal()
	{ return internal; }
};

class UserProcVariable : public ProcVariable
{
    String args, body;
public:
    UserProcVariable(char *name, char *a=NULL, char *b = NULL)
	: ProcVariable(name, 0), args(a), body(b)
    	{ }
    char *Args()
	{ return (char *)args;	}
    char *Body()
	{ return (char *)body;	}
    void Args(char *a)
	{ args = a;		}
    void Body(char *b)
	{ body = b;		}
};

class InternalProcVariable : public ProcVariable
{
    prochandler_t handler;
    int na;
public:
    InternalProcVariable(char *name, prochandler_t h_in, int numargs)
	: ProcVariable(name, 1), handler(h_in), na(numargs)
    	{ }
    prochandler_t Handler()
	{ return handler; }
    void Handler(prochandler_t h)
	{ handler = h; }
    int NumArgs()
	{ return na; }
    void NumArgs(int n)
	{ na = n; }
};

class ProfileVariable : public Variable
{
    int cnt;
    unsigned long tm;
public:
    ProfileVariable(char *name)
	: Variable(PROFVAR, name), cnt(0), tm(0)
    	{ }
    void Profile(unsigned long t)
    {
	TraceThis();
	cnt++;
	tm += t;
    }
    int Count()
	{ return cnt; }
    int Mean()
	{ return cnt?(int)(tm/cnt):0; }
};

class VarStack
{
    VariableTree	*(v[MAX_NEST]);
    StringGroup		*(args[MAX_NEST]);
    int pos;
public:
    VarStack();
    ~VarStack();
    void Push(VariableTree *vt);
    VariableTree *Pop();
    void EnterFrame(StringGroup *args);
    StringGroup *GetArgs(int lvl)
	{ return args[lvl]; }
    void ExitFrame();
    VariableTree *Frame(int lvl = 0);
    int Depth()
	{ return pos; }
};

// Prototypes from vars.cpp

ArrayVariable *GetArray(char *name);
char *GetVar(char *name, char *index = NULL);
Variable *SetVar(char *val, char *name, char *index = NULL);
int DestroyVar(char *name, char *index = NULL);

void EnterFrame(StringGroup *args);
void ExitFrame();
void UpLevel(VarStack &v, int lvl);
void DownLevel(VarStack &v);

extern int MakeAlias(int lvl, char *gnm, char *gidx, char *lnm, char *lidx);

void AddTrace(char *name, char *index, unsigned f, char *handler);
void DelTrace(char *name, char *index, unsigned f, char *handler);
void TraceInfo(char *name, char *index, String &res);

// Execution profiling

void UpdateProfile(char *name, unsigned long tm);
void ShowProfiles();

// Procedure defining, undefining, renaming, and lookup

ProcVariable *LookupProc(char *name);
ProcVariable *DefineProc(char *name, char *args, char *body);
ProcVariable *DefineProc(char *name, prochandler_t handler, int numargs);
void RemoveProc(char *name);
void RenameProc(char *name, char *newname);

// File id management

char *RecordFile(FILE *fp);
FILE *FindFile(char *name);
void DeleteFile(char *name);

// Array searching and info

char *StartSearch(ArrayVariable *av);
char *NextElement(char *srchid);
int AnyMore(char *srchid);
void StopSearch(char *srchid);

// Info handling

void GetGlobals(String &res, char *pat = NULL);
void GetLocals(String &res, char *pat = NULL);
void GetCommands(String &res, char *pat = NULL);
void GetProcs(String &res, char *pat = NULL);
void GetLevelInfo(String &res, int lvl=-1);
void GetVars(String &res, char *pat = NULL);

// Startup/shutdown

void InitVars();
void SetupPredefVars(int argc, char *argv[]);
void FreeVars();

#endif

