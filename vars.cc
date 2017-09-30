#include "tcl.h"

VariableTree	*proctree = NULL;
VariableTree	*filetree = NULL;
VariableTree	*tracetree = NULL;
VariableTree	*profiletree = NULL;
VariableTree	*searchtree = NULL;

// Variable handling

void TraceableVariable::Untrace(unsigned f, char *h)
{
    Tracer *tmp = t, *last = NULL;
    while (tmp)
    {
        if (tmp->Flags()==f && strcmp(tmp->Handler(), h)==0)
        {
    	    if (last) last->Next(tmp->Next());
    	    else t = tmp->Next();
    	    delete t;
    	    break;
        }
        tmp = tmp->Next();
    }
}

char *Tracer::Ops()
{
    static char ops[4], *p = ops;
    if (flags & TRACE_SET) *p++ = 'w';
    if (flags & TRACE_GET) *p++ = 'r';
    if (flags & TRACE_UNSET) *p++ = 'u';
    *p++=0;
    return ops;
}

void Tracer::TraceInfo(String &res)
{
    String tr(Ops());
    tr += (char)' ';
    tr += Handler();
    AddListElt(res, tr);
    if (next) next->TraceInfo(res);
}

void TraceableVariable::TraceInfo(String &res)
{
    t->TraceInfo(res);
}

Tracer::~Tracer()
{
    if (next) delete next;
}

TraceableVariable::~TraceableVariable()
{
    if (t) delete t;
}

// We keep the activation records (trees) in a stack

VarStack frames;

VarStack::VarStack()
{
    TraceThis();
    for (int i = 1; i < MAX_NEST; i++)
    	v[i] = NULL;
    v[0] = new VariableTree;
    pos = 0;
}

VarStack::~VarStack()
{
    TraceThis();
    for (int i = 0; i < MAX_NEST; i++)
    	if (v[i]) delete v[i];
}

void VarStack::Push(VariableTree *vt)
{
    TraceThis();
    if (++pos < MAX_NEST)
    {
    	assert(v[pos]==NULL);
    	v[pos] = vt;
    }
    else
    {
    	fputs("Stack overflow", stderr);
    	exit(-1);
    }
}

VariableTree *VarStack::Pop()
{
    TraceThis();
    if (pos<=0)
    {
    	fputs("Stack underflow", stderr);
    	exit(-1);
    }
    VariableTree *rtn = v[pos];
    v[pos--] = NULL;
    return rtn;
}

void VarStack::EnterFrame(StringGroup *a)
{
    TraceThis();
    if (++pos < MAX_NEST)
    {
    	assert(v[pos]==NULL);
    	v[pos] = new VariableTree;
	args[pos] = a;
    }
    else
    {
    	fputs("Stack overflow", stderr);
    	exit(-1);
    }
}

void VarStack::ExitFrame()
{
    TraceThis();
    if (pos<=0)
    {
    	fputs("Stack underflow", stderr);
    	exit(-1);
    }
    delete v[pos];
    v[pos--] = NULL;
}

VariableTree *VarStack::Frame(int lvl)
{
    TraceThis();
    if (lvl < 0) lvl = pos;
    assert((pos-lvl)>=0);
    return v[pos-lvl];
}

//-------------------------------------------------------------------
// Variable tree

VariableTree::VariableTree()
{
    TraceThis();
    sentinel = new VarTreeEntry(new ScalarVariable(""));
    root = new VarTreeEntry(new ScalarVariable(""), NULL, sentinel);
    vcnt = 0;
}

void VariableTree::Trash(VarTreeEntry *p)
{
    TraceThis();
    if (p->l) Trash(p->l);
    if (p->r) Trash(p->r);
    if (p != sentinel) delete p;
}

VariableTree::~VariableTree()
{
    TraceThis();
    Trash(root);
    delete sentinel;
}

int WalkNode(VarTreeEntry *p, VarTreeEntry *s, Walker w, void *arg)
{
    if (p && p!=s)
    {
        if ((*w)(p->v, arg) != 0) return -1;
	WalkNode(p->l, s, w, arg);
	WalkNode(p->r, s, w, arg);
    }
    return 0;
}

int VariableTree::Walk(Walker w, void *arg)
{
    TraceThis();
    return WalkNode(root->r, sentinel, w, arg);
}

int VariableTree::Delete(char *name, char *array)
{
    TraceThis();
    VarTreeEntry *c, *p, *x, *t;
    sentinel->v->Rename(name);
    p = root; x = root->r;
    for (;;)
    {   
	int cmp = strcmp((char *)x->Name(), name);
	if (cmp==0) break;
	p = x;
	x = (cmp>0) ? x->l : x->r;
    }
    if (x==sentinel)
	return -1; // not found
    else
    {
	Variable *v = x->v;
	Tracer *trc = NULL;
	if (v->IsTraceable())
	{
	    trc = ((TraceableVariable *)v)->GetTrace();
	    if (trc)
	    {
	    	if (array)
	    	{
		    if (trc->Handle(TRACE_UNSET, array, name) != 0)
		        return -1;
	        }
	        else if (trc->Handle(TRACE_UNSET, name) != 0)
		    return -1;
	    }
	}
	t = x;
	if (t->r == sentinel) x = x->l;
	else if (t->r->l == sentinel)
	{
	    x = x->r;
	    x->l = t->l;
	}
	else
	{
	    c = x->r;
	    while (c->l->l != sentinel) c = c->l;
	    x = c->l; c->l = x->r;
	    x->l = t->l;
	    x->r = t->r;
	}
	vcnt--;
	delete t;
	if (strcmp((char *)p->Name(), name) > 0)
	    p->l = x;
	else
	    p->r = x;
    }
    return 0;
}

void VariableTree::AddVar(Variable *v)
{
    TraceThis();
    VarTreeEntry *varent = root->r, *last = root;
    sentinel->v->Rename(v->Name());
    for (;;)
    {
        int cmp = strcmp((char *)varent->Name(), v->Name());
        if (cmp==0)
            break;
	last = varent;
	varent = (cmp>0) ? varent->l : varent->r;
    }
    assert(varent == sentinel); // should be error return !!
    varent = new VarTreeEntry(v, sentinel, sentinel);
    if (strcmp((char *)last->Name(), v->Name()) > 0)
        last->l = varent;
    else
        last->r = varent;
    vcnt++;
}

Variable *VariableTree::Fetch(char *name)
{
    TraceThis();
    VarTreeEntry *varent = root->r;
    sentinel->v->Rename(name);
    for (;;)
    {
        int cmp = strcmp((char *)varent->Name(), name);
        if (cmp==0) break;
	varent = (cmp>0) ? varent->l : varent->r;
    }
    if (varent == sentinel)
	return NULL;
    return varent->v;
}

Variable *VariableTree::Search(char *name)
{
    Variable *v = Fetch(name);
    return (v && v->IsAlias()) ? ((AliasVariable *)v)->Real() : v;
}

//-----------------------------------------------------------
// Array info

static char *array_opts[] =
{
    "anymore",
    "donesearch",
    "names",
    "nextelement",
    "size",
    "startsearch"
};

// hax

static int nopat;
static char *arrname = NULL;

static int AppendMatchingElt(Variable *v, void *arg)
{
    String &s = *(String *)arg;
    if (nopat || GlobMatch(v->Name()))
    {
	if (v->IsArray())
	{
	    arrname = v->Name();
    	    ((ArrayVariable*)v)->Elements()->Walk(AppendMatchingElt, arg);
	    arrname = NULL;
	}
	else if (arrname)
	{
	    String tmp(arrname);
	    tmp += (char )'(';
	    tmp += v->Name();
	    tmp += (char )')';
	    AddListElt(s, tmp);
	}
	else AddListElt(s, v->Name());
    }
    return 0;
}

int arrayH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY, na = args.Count();
    int opt = FindString(args.GetString(1), array_opts, 
			sizeof(array_opts)/sizeof(array_opts[0]));
    char *txt;
    ArrayVariable *av = GetArray((char *)args.GetString(2));
    if (av==NULL)
    {
	result = args.GetString(2) + " is undefined or not an array";
	rtn = ERROR;
    }
    else switch (opt) // very crude handling with almost no error checking for now
    {
    default:
        result = "Bad option arg in array command";
	rtn = ERROR;
	break;
    case 0:	// anymore
	if (na != 4) goto badargs;
	result = (int)AnyMore((char *)args.GetString(3));
	break;
    case 1:	// donesearch
	if (na != 4) goto badargs;
	StopSearch((char *)args.GetString(3));
	break;
    case 2:	// names

	nopat = 1;
        av->Elements()->Walk(AppendMatchingElt, &result);
	break;
    case 3:	// nextelement
	if (na != 4) goto badargs;
	result = NextElement((char *)args.GetString(3));
	break;
    case 4:	// size
	if (na != 3) goto badargs;
	result = (int)av->Elements()->Count();
	break;
    case 5:	// startsearch
	if (na != 3) goto badargs;
	result = StartSearch(av);
	break;
    }
    return rtn;
badargs:
    result = "wrong # args in array command";
    return ERROR;
}

//----------------------------------------------------------------
// Variable set/unset

static Variable *LookupVar(VariableTree *t, char *name, char *index = NULL)
{
    Variable *v = t->Search(name);
    if (v && index)
    {
	if (v->IsArray())
	    v = ((ArrayVariable *)v)->Elements()->Search(index);
	else v = NULL;
    }
    return v;
}

char *FindVar(VariableTree *t, char *name, char *index)
{
    Variable *v = LookupVar(t, name, index);
    if (v && v->IsNormal())
    {
	Tracer *t = ((TraceableVariable *)v)->GetTrace();
	if (t)
	    if (t->Handle(TRACE_GET, name) != 0)
		return NULL;
    	return ((ScalarVariable *)v)->GetContents();
    }
    return NULL;
}

char *GetVar(char *name, char *index)
{
    return FindVar(frames.Frame(), name, index);
}

ArrayVariable *GetArray(char *name)
{
    ArrayVariable *av = (ArrayVariable*)LookupVar(frames.Frame(), name);
    return (av->IsArray()) ? av : NULL;
}

static Variable *WriteVar(VariableTree *t, char *val, char *name, char *index)
{
    TraceThis();
    Variable *v = t->Search(name);
    if (v == NULL) // create and set
    {
	if (index)
	{
	    ArrayVariable *av = new ArrayVariable(name);
	    t->AddVar(av);
	    t = av->Elements();
	    t->AddVar(v = new ScalarVariable(index, val));
	}
	else t->AddVar(v = new ScalarVariable(name, val));
    }
    else if (index)
    {
	if (!v->IsArray())
	{
	    // convert scalar to array
	    t->Delete(name);
	    t->AddVar(v = new ArrayVariable(name));
	}
	t = ((ArrayVariable *)v)->Elements();
	v = t->Search(index);
	if (v==NULL)
	    t->AddVar(v = new ScalarVariable(index, val));
	else
	{
	    ((ScalarVariable *)v)->Set(val);
	    Tracer *trc = ((TraceableVariable *)v)->GetTrace();
	    if (trc)
	        if (trc->Handle(TRACE_SET, name, index) != 0)
		    return NULL;
	}
    }
    else if (v->IsNormal())
    {
	((ScalarVariable *)v)->Set(val);
	Tracer *trc = ((TraceableVariable *)v)->GetTrace();
	if (trc)
	    if (trc->Handle(TRACE_SET, name) != 0)
		return NULL;
    }
    else return NULL; // error
    return v;
}

Variable *SetVar(char *val, char *name, char *index)
{
    return WriteVar(frames.Frame(), val, name, index);
}

static int DestroyVar(VariableTree *t, char *name, char *index = NULL)
{
    if (index)
    {
	Variable *av = t->Search(name);
	if (av && av->IsArray())
	    return ((ArrayVariable *)av)->Elements()->Delete(index, name);
	else return -1;
    }
    else return t->Delete(name);
}

int DestroyVar(char *name, char *index)
{
    Variable *v = (AliasVariable*)frames.Frame()->Fetch(name);
    AliasVariable *av;
    if (v==NULL) return -1;
    // If aliases are involved, need to delete the real var too...
    if (v->IsAlias())
    {
	av = (AliasVariable*)v;
	int lvl = frames.Depth() - av->Level();
	if (av->Array()) // name aliased to array elt
	{
	    if (index) return -1; // can't have another index!
	    else
	    {
		// delete the array element
		(void)DestroyVar(frames.Frame(lvl),
			av->Array()->Name(), av->Real()->Name());
	    }
	}
	else // name aliased to scalar or array
	{
	    if (index) // name was aliased to an array
	    {
		// this case is handled in DestroyVar(t, n, i)
	    }
	    else // name aliased to a scalar
	    {
		// delete the scalar
		(void)DestroyVar(frames.Frame(lvl), av->Real()->Name());
	    }
	}
    }
    else if (index)
    {
	if (!v->IsArray()) return -1;
	av = (AliasVariable*)((ArrayVariable*)v)->Elements()->Fetch(index);
    	if (av==NULL) return -1;
        if (av->IsAlias())
	{
	    int lvl = frames.Depth() - av->Level();
	    if (av->Array())
	    {
	        // deleting an elt aliased to an elt
		(void)DestroyVar(frames.Frame(lvl),
			av->Array()->Name(), av->Real()->Name());
	    }
	    else
	    {
	        // deleting an elt aliased to a scalar
		(void)DestroyVar(frames.Frame(lvl), av->Real()->Name());
	    }
	}
    }
    // destroy the actual variable
    return DestroyVar(frames.Frame(), name, index);
}

//----------------------------------------------------------------
// Aliasing
//

int MakeAlias(int lvl, char *gname, char *gindex, char *lname, char *lindex)
{
    TraceThis();
    int level = lvl;
    if (lvl < 0) lvl = frames.Depth(); // global
    Variable *rv = LookupVar(frames.Frame(lvl), gname, gindex);
    if (rv == NULL)
	rv = WriteVar(frames.Frame(lvl), "", gname, gindex); // create it

    Variable *av = gindex ? LookupVar(frames.Frame(lvl), gname) : NULL;
    Variable *lv = frames.Frame()->Search(lname);
    if (lindex)
    {
	if (lv == NULL)
	{
	    frames.Frame()->AddVar(lv = new ArrayVariable(lname));
	    ((ArrayVariable*)lv)->Elements()->AddVar(new AliasVariable(lindex,rv, level, av));
	}
	else
	{
	    VariableTree *e = ((ArrayVariable*)lv)->Elements();
	    lv = e->Search(lindex);
	    if (lv == NULL)
		e->AddVar(new AliasVariable(lindex,rv, level, av));
	    else return -1;
	}
    }
    else if (lv==NULL)
	frames.Frame()->AddVar(new AliasVariable(lname, rv, level, av));
    else return -1;
    return 0;
}

//----------------------------------------------------------------
// Variable tracing
//

int Tracer::Handle(unsigned evt, char *name, char *index)
{
    if ((evt & flags) != 0)
    {
	if (flags & TRACE_BUSY) return 0; // prevent recursive invocations
	flags |= TRACE_BUSY;
	String is(handler), res;
    	is += ' '; is += name; is += ' ';
	if (index) is += index;
	else is += "{}";
	is += ' ';
	if (evt==TRACE_SET) is += 'w';
	else if (evt==TRACE_GET) is += 'r';
	else is += 'u';
	if (Interpret(&is, res)==ERROR)
	{
	    flags &= ~TRACE_BUSY;
	    return -1;
	}
	flags &= ~TRACE_BUSY;
    }
    else if (next)
	return next->Handle(evt, name, index);
    return 0;
}

void AddTrace(char *name, char *index, unsigned f, char *handler)
{
    TraceThis();
    Variable *v = LookupVar(frames.Frame(), name, index);
    if (v==NULL)
	v = WriteVar(frames.Frame(), "", name, index); // create it
    if (v && v->IsTraceable())
	((TraceableVariable *)v)->Trace(f, handler);
}

void DelTrace(char *name, char *index, unsigned f, char *handler)
{
    TraceThis();
    Variable *v = LookupVar(frames.Frame(), name, index);
    if (v && v->IsTraceable())
	((TraceableVariable *)v)->Untrace(f, handler);
}

void TraceInfo(char *name, char *index, String &res)
{
    TraceThis();
    Variable *v = LookupVar(frames.Frame(), name, index);
    if (v && v->IsTraceable())
	((TraceableVariable *)v)->TraceInfo(res);
}

//-------------------------------------------------------------------
// Stack frame management

void EnterFrame(StringGroup *a)
{
    frames.EnterFrame(a);
}

void ExitFrame()
{
    frames.ExitFrame();
}

void UpLevel(VarStack &v, int lvl)
{
    while (lvl--) v.Push(frames.Pop());
}

void DownLevel(VarStack &v)
{
    int lvl = v.Depth(); 
    while (lvl--) frames.Push(v.Pop());
}

//------------------------------------------------------------
// File id tree management

static int fcnt = 0;

char *RecordFile(FILE *fp)
{
    char nm[8];
    sprintf(nm, "fp%d", fcnt++);
    FileVariable *v = new FileVariable(nm, fp);
    filetree->AddVar(v);
    return (char *)v->Name();
}

FILE *FindFile(char *name)
{
    FileVariable *fv = (FileVariable *)filetree->Search(name);
    return fv ? fv->GetFP() : NULL;
}

void DeleteFile(char *name)
{
    filetree->Delete(name);
}

//------------------------------------------------------------------
// Procedures

ProcVariable *LookupProc(char *name)
{
    return (ProcVariable *)proctree->Search(name);
}

ProcVariable *DefineProc(char *name, prochandler_t handler, int numargs)
{
    ProcVariable *pv = (ProcVariable *)proctree->Search(name);
    if (pv == NULL)
	proctree->AddVar(pv = new InternalProcVariable(name, handler, numargs));
    else
    {
	((InternalProcVariable *)pv)->Handler(handler);
	((InternalProcVariable *)pv)->NumArgs(numargs);
    }
    return pv;
}

ProcVariable *DefineProc(char *name, char *args, char *body)
{
    ProcVariable *pv = (ProcVariable *)proctree->Search(name);
    if (pv == NULL)
	proctree->AddVar(pv = new UserProcVariable(name, args, body));
    else
    {
	((UserProcVariable *)pv)->Args(args);
	((UserProcVariable *)pv)->Body(body);
    }
    return pv;
}

void RemoveProc(char *name)
{
    proctree->Delete(name);
}

void RenameProc(char *name, char *newname)
{
    ProcVariable *pv = (ProcVariable *)proctree->Search(name);
    if (pv)
    {
	if (pv->IsInternal())
	{
	    prochandler_t h = ((InternalProcVariable*)pv)->Handler();
	    int na = ((InternalProcVariable*)pv)->NumArgs();
	    (void)DefineProc(newname, h, na);
	}
	else
	{
	    char *a = ((UserProcVariable*)pv)->Args();
	    char *b = ((UserProcVariable*)pv)->Body();
	    (void)DefineProc(newname, a, b);
	}
	RemoveProc(name);
    }
}

//------------------------------------------------------------------
// Execution profiling

void UpdateProfile(char *name, unsigned long tm)
{
    ProfileVariable *v = (ProfileVariable *)profiletree->Search(name);
    if (v==NULL)
    	profiletree->AddVar(v = new ProfileVariable(name));
    v->Profile(tm);
}

static int ShowProf(Variable *v, void *arg)
{
    (void)arg;
    printf("Proc %-16s executed %-4d times (mean %-3d msecs)\n",
	(char *)v->Name(), ((ProfileVariable *)v)->Count(),
		((ProfileVariable *)v)->Mean());
    return 0;
}

void ShowProfiles()
{
    profiletree->Walk(ShowProf);
}

//-------------------------------------------------------------------
// Array searching is a problem as we don't maintain
// a recursion stack, so we have to search from the beginning
// each time next is called 8-(. It is better to use foreach
// for now until a solution is found (which may mean discarding
// binary trees...)

int SearchVariable::AnyMore()
{
    return 0;    
}

char *SearchVariable::Next()
{
    return "";
}

static int scnt = 0;

char *StartSearch(ArrayVariable *av)
{
    char nm[8];
    sprintf(nm, "srch%d", scnt++);
    SearchVariable *v = new SearchVariable(nm, av->Elements());
    searchtree->AddVar(v);
    return (char *)v->Name();
}

char *NextElement(char *srchid)
{
    SearchVariable *sv = (SearchVariable *)searchtree->Search(srchid);
    return (sv==NULL) ? "" : sv->Next();
}

int AnyMore(char *srchid)
{
    SearchVariable *sv = (SearchVariable *)searchtree->Search(srchid);
    return (sv==NULL) ? 0 : sv->AnyMore();
}

void StopSearch(char *srchid)
{
    searchtree->Delete(srchid);
}

//-------------------------------------------------------------------
// info routines

void GetGlobals(String &res, char *pat)
{
    nopat = pat ? 0 : 1;
    if (pat)
    {
	nopat = 0;
	SetGlobPattern(pat);
    }
    else nopat = 1;
    frames.Frame(-1)->Walk(AppendMatchingElt, &res);
}

static int AppendMatchingLocal(Variable *v, void *arg)
{
    String &s = *(String *)arg;
    if (nopat || GlobMatch(v->Name()))
    {
	if (!v->IsAlias() || ((AliasVariable *)v)->Level() == frames.Depth()) 
            AddListElt(s, v->Name());
    }
    return 0;
}

void GetLocals(String &res, char *pat)
{
    if (frames.Depth()>0)
    {
	nopat = pat ? 0 : 1;
	if (pat)
	{
	    nopat = 0;
	    SetGlobPattern(pat);
	}
	else nopat = 1;
	frames.Frame()->Walk(AppendMatchingLocal, &res);
    }
}

void GetCommands(String &res, char *pat)
{
    nopat = pat ? 0 : 1;
    if (pat)
    {
	nopat = 0;
	SetGlobPattern(pat);
    }
    else nopat = 1;
    proctree->Walk(AppendMatchingElt, &res);
}

static int AppendMatchingProc(Variable *v, void *arg)
{
    String &s = *(String *)arg;
    if ((nopat || GlobMatch(v->Name())) && !((ProcVariable *)v)->IsInternal())
        AddListElt(s, v->Name());
    return 0;
}

void GetProcs(String &res, char *pat)
{
    nopat = pat ? 0 : 1;
    if (pat)
    {
	nopat = 0;
	SetGlobPattern(pat);
    }
    else nopat = 1;
    proctree->Walk(AppendMatchingProc, &res);
}

void GetLevelInfo(String &res, int lvl)
{
    if (lvl < 0) res = frames.Depth();
    else if (lvl == 0) // get argv array
    {
	res = GetVar("argv0");
	res += (char)' ';
	res += GetVar("argv");
    }
    else
    {
	StringGroup *a = frames.GetArgs(lvl);
	for (int arg = 0; arg < a->Count(); arg++)
	    AddListElt(res, a->GetString(arg));
    }
}

void GetVars(String &res, char *pat)
{
    nopat = pat ? 0 : 1;
    if (pat)
    {
        nopat = 0;
        SetGlobPattern(pat);
    }
    else nopat = 1;
    frames.Frame()->Walk(AppendMatchingElt, &res);
}

//-------------------------------------------------------------------

void SetupPredefVars(int argc, char *argv[])
{
    String tmp;
    extern char **environ;
    for (int i = 0; environ[i]; i++)
    {
	tmp = environ[i];
	char *v = strchr((char *)tmp, '=');
	assert(v);
	*v++=0;
	SetVar(v, "env", (char *)tmp);
    }
    tmp = (int)(argc-1);
    SetVar((char *)tmp, "argc");
    SetVar(argv[0], "argv0");
    tmp = "";
    for (int i = 1; i < argc; i++)
    {
	String tmp2(argv[i]);
	AddListElt(tmp, tmp2);
    }
    SetVar((char *)tmp, "argv");
}

// misnomers, these...

void InitVars()
{
    TraceThis();
    proctree = new VariableTree;
    profiletree = new VariableTree;
    filetree = new VariableTree;
    tracetree = new VariableTree;
    searchtree = new VariableTree;
    filetree->AddVar(new FileVariable("stdin", stdin));
    filetree->AddVar(new FileVariable("stdout", stdout));
    filetree->AddVar(new FileVariable("stderr", stderr));
}

void FreeVars()
{
    TraceThis();
    delete searchtree;
    delete proctree;
    delete filetree;
    delete tracetree;
    delete profiletree;
}
