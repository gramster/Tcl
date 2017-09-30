#include "tcl.h"

#ifdef DEBUG
long strcount = 0;
#endif

#ifdef TRON

dbg_stk_entry dbg_stk[DBG_STK];
int dbg_stk_pos = 0;

void ShowDbgStk(char *msg, void *arg)
{
    int i = dbg_stk_pos;
    if (msg)
	{ fprintf(stderr, msg, arg);  fprintf(stderr, " - call stack is:\n\n"); }
    else 
	  fprintf(stderr, "Call stack is:\n\n", msg);
    while (i--)
	fprintf(stderr, "%14s %d\n", dbg_stk[i].file, dbg_stk[i].line);
    fprintf(stderr, "\n");
}

void PushDbgStk(char *f, int l)
{
    assert(dbg_stk_pos < DBG_STK);
    strcpy(dbg_stk[dbg_stk_pos].file, f);
    dbg_stk[dbg_stk_pos++].line = l;
}

void PopDbgStk()
{
    assert(dbg_stk_pos>0);
    dbg_stk_pos--;
}

#endif

void FailAssert(char *msg, char *fnm, int ln)
{
    TraceThis();
    fprintf(stderr, "Assertion failed: %s in %s line %d\n", msg, fnm, ln);
#ifdef DEBUG
    fprintf(stderr, "String count: %d\n", strcount);
#endif
    exit(-1);
}

// missing string functions; for now we just hack 'em pretty
// inefficiently (assuming a search from the back would be
// faster in typical use of "string last")

char *strrstr(char *s1, char *s2)
{
    char *rtn = NULL, *tmp = strstr(s1, s2);
    while (tmp)
    {
	rtn = tmp;
	tmp = strstr(tmp+1, s2);
    }
    return rtn;
}

size_t strrspn(char *s1, char *s2)
{
    int l = strlen(s1);
    while (--l >= 0)
    {
	if (strchr(s2, s1[l])==NULL)
	    break;
    }
    return l+1;
}

size_t strrcspn(char *s1, char *s2)
{
    int l = strlen(s1);
    while (--l >= 0)
    {
	if (strchr(s2, s1[l])!=NULL)
	    break;
    }
    return l+1;
}

void String::Expand(int newsz, int keep)
{
    char *newtext = (newsz > SMALLSTRING) ? (new char [newsz]) : data;
    if (keep)
        strncpy(newtext, text, newsz-1); // is -1 needed??
    if (sz > SMALLSTRING)
        delete [] text;
    text = newtext;
    sz = newsz;
}

// The next is probably too crude at present for subtlety

void String::AppendElement(char *string, int len)
{
    TraceThis();
    char *tmp = string;
    while (*tmp && !isspace(*tmp)) tmp++;
    if (text[len]!='{') AppendChar(' ');
    if (*tmp)
    {
	AppendChar('{');
	AppendStr(string, len);
	AppendChar('}');
    }
    else AppendStr(string, len);
}

int String::IsNumeric(int *val)
{
    TraceThis();
    int pos = 0, v = 0, sgn = 1;
    while (pos < len && isspace(text[pos]))
	pos++;
    if (pos < len)
	if (text[pos]=='-')
	{
	    pos++;
	    sgn = -1;
	}
    if (isdigit(text[pos]))
    {
	while (pos < len && isdigit(text[pos]))
	{
	    v = v * 10 + text[pos] - '0';
	    pos++;
	}
	if (pos == len)
	{
	    *val = sgn * v;
	    return 1;
	}
    }
    return 0;
}

int String::IsNumeric(long *val)
{
    TraceThis();
    int pos = 0;
    long v = 0, sgn = 1l;
    while (pos < len && isspace(text[pos]))
	pos++;
    if (pos < len)
	if (text[pos]=='-')
	{
	    pos++;
	    sgn = -1l;
	}
    if (isdigit(text[pos]))
    {
	while (pos < len && isdigit(text[pos]))
	{
	    v = v * 10l + text[pos] - '0';
	    pos++;
	}
	if (pos == len)
	{
	    *val = sgn * v;
	    return 1;
	}
    }
    return 0;
}

void StringGroup::Expand(int newsize)
{
    TraceThis();
    if (newsize >= size)
    {
	int newsz = newsize + (16-newsize%16);
	String *newstr = new String[newsz];
	assert(newstr);
	for (int i=0; i<elts;i++)
	    newstr[i] = str[i];
	delete [] str;
	str = newstr;
	size = newsz;
    }
}

void SortableStringGroup::Expand(int newsize)
{
    TraceThis();
    if (newsize >= size)
    {
	int newsz = newsize + (16-newsize%16);
	int *newidx = new int[newsz];
	assert(newidx);
	for (int i=0; i<Count();i++)
	    newidx[i] = idxtbl[i];
	delete [] idxtbl;
	idxtbl = newidx;
	StringGroup::Expand(newsize);
    }
}

void StringGroup::SetString(int idx, String &val)
{
    TraceThis();
    assert(idx>=0);
    Expand(idx+1);
    if (elts <= idx)
	elts = idx+1;
    str[idx] = val;
}

void StringGroup::SetString(int idx, const char *val)
{
    TraceThis();
    assert(idx>=0);
    Expand(idx+1);
    if (elts <= idx)
	elts = idx+1;
    str[idx] = val;
}

void StringGroup::Dump(String &trace)
{
    TraceThis();
    trace += (char)'"';
    for (int i = 0; i < elts; i++)
    {
	if (i) trace += (char)' ';
	trace += str[i];
    }
    trace += (char)'"';
    trace += (char)'\n';
}

//--------------------------------------------------------

static int cmpkey = ASCII_KEY, cmpup = 1;
static char *custom;
static SortableStringGroup *sg;

typedef int (*cmpFunc)(char *, char *);

static int ASCIICmp(char *a, char *b)
{
    return strcmp(a, b);
}

static int IntegerCmp(char *a, char *b)
{
    return atoi(a) - atoi(b);
}

static int RealCmp(char *a, char *b)
{
    float diff = atof(a) - atof(b);
    if (diff == 0.) return 0;
    else if (diff < 0.) return -1;
    else return 1;
}


static int CustomCmp(char *a, char *b)
{
    String cmd(custom), result;
    cmd += (char)' ';
    cmd += a;
    cmd += (char)' ';
    cmd += b;
    (void)Interpret(&cmd, result);
    int rtn;
    if (result.IsNumeric(&rtn))
	return rtn;
    else return 0; // should be error
}

static cmpFunc cmpTbl[] =
{
    ASCIICmp,
    IntegerCmp,
    RealCmp,
    CustomCmp
};

static int fcmp(const void *a, const void *b)
{
    int v = (*(cmpTbl[cmpkey]))((char *)sg->StringGroup::GetString(*(int *)a),
				(char *)sg->StringGroup::GetString(*(int *)b));
    if (!cmpup) v = -v;
    return v;
}

#if 0
// SortableStringGroup specific bubble sorter. Useful for
// debugging now and then.

static int fcmp(const int *a, const int *b)
{
    int v = (*(cmpTbl[cmpkey]))((char *)sg->StringGroup::GetString(*a),
				(char *)sg->StringGroup::GetString(*b));
    if (!cmpup) v = -v;
    return v;
}

void bubsort(int *base, size_t cnt, size_t len, int (*fcmp)(const int *a, const int *b))
{
    for (int i = 0 ; i < (cnt-1); i ++)
	for (int j = i+1; j < cnt; j++)
	{
	    printf("Comparing %s (%d) and %s (%d)\n",
			(char *)sg->StringGroup::GetString(base[i]), base[i], 
			(char *)sg->StringGroup::GetString(base[j]), base[j]);
	    int v = fcmp(&base[i], &base[j]);
	    if (v>0)
	    {
	        printf("Compare > 0; swapping\n");
		int tmp = base[i];
		base[i] = base[j];
		base[j] = tmp;
	    }
	    printf("After compare, indices are: ");
	    for (int k = 0; k < cnt; k++)
		printf("%d ", base[k]);
	    printf("\n");
	    printf("               strings are: ");
	    for (k = 0; k < cnt; k++)
		printf("%s ", (char *)sg->GetString(k));
	    printf("\n");
	}
}
#endif
void SortableStringGroup::Sort(int key, int increasing, char *cust)
{
    if (Count()>1)
    {
	cmpkey = key;
        cmpup = increasing;
	custom = cust;
	sg = this;
#if 1
        qsort((void *)idxtbl,(size_t)Count(), sizeof(int), fcmp);
#else
        bubsort(idxtbl,(size_t)Count(), sizeof(int), fcmp);
#endif
    }
}

//-----------------------------------------------------------
// String handling


static char *string_opts[] =
{
    "compare",
    "first",
    "index",
    "last",
    "length",
    "match",
    "range",
    "tolower",
    "toupper",
    "trim",
    "trimleft",
    "trimright",
    "trunc"
};

int trimString(StringGroup &args, int left, int right, String &res)
{
    char *sep;
    if (args.Count()==4)
	sep = args.GetString(3);
    else if (args.Count()<3 || args.Count()>4)
    {
	res = "Bad # of args in string trim";
	return ERROR;
    }
    else sep = " \t\n\r";
    char *str = args.GetString(2);
    int start = left ? strspn(str, sep) : 0;
    int end = right ? strrspn(str, sep) : strlen(str);
    res = &str[start];
    res.Trunc(end-start);
    return OKAY;
}

int stringH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY, na = args.Count();
    int opt = FindString(args.GetString(1), string_opts, 
			sizeof(string_opts)/sizeof(string_opts[0]));
    char *txt;
    switch (opt) // very crude handling with almost no error checking for now
    {
    default:
        result = "Bad option arg in string command";
	rtn = ERROR;
	break;
    case 0:	// compare
	if (na != 4) goto badargs;
	else
	{
	    int cmp = strcmp((char *)args.GetString(2), (char *)args.GetString(3));
	    if (cmp<0) result = -1;
	    else if (cmp>0) result = 1;
	    else result = "0";
	}
	break;
    case 1:	// first
	if (na != 4) goto badargs;
	else
	{
	    char *s1 = args.GetString(3);
	    char *s2 = strstr(s1, (char *)args.GetString(2));
	    if (s2)
	        result = (int)(s2-s1);
	    else
	        result = -1;
	}
	break;
    case 2:	// index
	if (na != 4) goto badargs;
	else
	{
	    int idx;
	    if (!args.GetString(3).IsNumeric(&idx))
	    {
                result = "Bad index number in string index command";
	        rtn = ERROR;
	    }
	    else if (idx>0 && idx<args.GetString(2).Length())
	        result = (char)(args.GetString(2)[idx]);
	    break;
	}
    case 3:	// last
	if (na != 4) goto badargs;
	else
	{
	    char *s1 = args.GetString(3);
	    char *s2 = strrstr(s1, (char *)args.GetString(2));
	    if (s2)
	        result = (int)(s2-s1);
	    else
	        result = -1;
	}
	break;
    case 4:	// length
	if (na != 3) goto badargs;
	else result = (int)strlen((char *)args.GetString(2));
	break;
    case 5:	// match
	if (na != 4) goto badargs;
	else
	{
	    SetGlobPattern(args.GetString(2));
	    result = GlobMatch(args.GetString(3));
	}
	break;
    case 6:	// range
	if (na != 5) goto badargs;
	else
	{
	    int start, end;
	    if (strcmp((char *)args.GetString(4), "end")==0)
	        end = args.GetString(2).Length()-1;
	    else if (!args.GetString(4).IsNumeric(&end))
	    {
	        result = "Bad end argument in string range command";
	        rtn = ERROR;
	    }
	    if (!args.GetString(3).IsNumeric(&start))
	    {
	        result = "Bad start argument in string range command";
	        rtn = ERROR;
	    }
	    else if (start <= end)
	    {
	        result = ((char *)args.GetString(2))+start;
	        result.Trunc(end-start+1);
	    }
	}
	break;
    case 7:	// tolower
	if (na != 3) goto badargs;
	else
	{
	    result = args.GetString(2);
	    txt = (char *)result;
	    for (int i = result.Length(); i-- >= 0; )
		    txt[i] = tolower(txt[i]);
	}
	break;
    case 8:	// toupper
	if (na != 3) goto badargs;
	else
	{
	    result = args.GetString(2);
	    txt = (char *)result;
	    for (int i = result.Length(); i-- >= 0; )
		    txt[i] = toupper(txt[i]);
	}
	break;
    case 9:	// trim
	rtn = trimString(args, 1, 1, result);
	break;
    case 10:	// trimleft
	if ((rtn = trimString(args, 1, 0, result)) != OKAY)
	    result += "left";
	break;
    case 11:	// trimright
	if ((rtn = trimString(args, 0, 1, result)) != OKAY)
	    result += "right";
	break;
    case 12:	// trunc
	result = args.GetString(2);
	{
	    int v;
	    if (na < 4 || !args.GetString(3).IsNumeric(&v))
	        v = result.Length()-1;
	    if (v<0) v = 0;
	    if (v > result.Length()) v = result.Length();
	    result.Trunc(v);
	}
	break;
    }
    return rtn;
badargs:
    result = "wrong # args in string command";
    return ERROR;
}


