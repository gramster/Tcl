// Lists of various kinds!

#include "tcl.h"

// Line lists

void LineList::Empty()
{
    Line *tmp = head.Next();
    while (tmp != &head)
    {
	Line *tmp2 = tmp;
	tmp = tmp->Next();
	delete tmp2;
    }
    Init();
}

void LineList::Goto(int n)
{
    if (n<0) n = 0;
    else if (n>=cnt) n = cnt-1;
    if (now==NULL) { now = head.Next(); assert (pos==0); }
    int mov = n - pos;
    if (mov>0)
    {
        while (mov--)
        {
    	now = now->Next();
    	pos++;
        }
    } else while (mov++)
    {
        now = now->Prev();
        pos--;
    }
}

// Tcl lists

void AddListElt(String &res, char *elt, char *sep, int escape)
{
    TraceThis();
    StringGroup elts;
    int ne = SplitList(elt, elts, 2);
    int len = res.Length();
    int lastch = (len>0) ? res[len-1] : ' ';
    if (!isspace(lastch))
	res+=sep;
    if (ne!=1 || elt[0]==' ')
    {
	res+=(char)'{';
	res+=elt;
	res+=(char)'}';
    }
    else if (escape)
    {
	char *tp = elt;
	while (*tp)
	{
	    // should really use a table for speed!!
	    if (*tp=='{' || *tp=='}' || *tp=='[' || *tp==']' || *tp==' ' ||
	    	*tp=='"' || *tp=='\\' || *tp==';' || *tp=='\n')
		res += (char)'\\';
	    res += (char)(*tp++);
	}
    }
    else res+=elt;
}

void AddListElt(String &res, String &elt, char *sep, int escape)
{
    AddListElt(res, (char *)elt, sep, escape);
}

int lindexH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY, elt;
    if (!args.GetString(2).IsNumeric(&elt) || elt<0)
    {
        result = "bad element number in lindex";
        rtn = ERROR;
    }
    else
    {
        StringGroup elts;
        int ne = SplitList(args.GetString(1), elts, elt+1);
        if (ne != (elt+1))
        {
            result = "no such field in lindex";
            rtn = ERROR;
        }
        else result = elts.GetString(elt);
    }
    return rtn;
}

int llengthH(StringGroup &args, String &result)
{
    TraceThis();
    StringGroup elts;
    result = SplitList(args.GetString(1), elts, 0);
    return OKAY;
}

int linsertH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY, elt;
    if (!args.GetString(2).IsNumeric(&elt) || elt<0)
    {
        result = "bad element number in linsert";
        rtn = ERROR;
    }
    else
    {
        StringGroup elts;
        int i, ne = SplitList(args.GetString(1), elts, 0);
        if (elt>ne) elt = ne;
        result = "";
        for (i = 0; i < elt; i++)
    	AddListElt(result, elts.GetString(i));
        for (int j = 3; j < args.Count(); j++)
    	AddListElt(result, args.GetString(j));
        for (; i < ne; i++)
    	AddListElt(result, elts.GetString(i));
    }
    return rtn;
}

int lappendH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    String *base, *idx;
    char *id = SplitVar(args.GetString(1), base, idx);
    StringGroup elts;
    int ne = SplitList(GetVar((char *)*base, id), elts, 0);
    result = "";
    for (int i = 0; i < ne; i++)
        AddListElt(result, elts.GetString(i));
    for (int j = 2; j < args.Count(); j++)
        AddListElt(result, args.GetString(j));
    SetVar(result, (char *)*base, id);
    if (base) delete base;
    if (idx) delete idx;
    return rtn;
}

int concatH(StringGroup &args, String &result)
{
    TraceThis();
    result = "";
    for (int i = 1; i < args.Count(); i++)
    {
        StringGroup elts;
        int ne = SplitList(args.GetString(i), elts, 0);
        for (int j = 0; j < ne; j++)
	    AddListElt(result, elts.GetString(j));
    }
    return OKAY;
}

int splitH(StringGroup &args, String &result)
{
    TraceThis();
    char *sep = NULL;
    char *p = (char *)args.GetString(1);
    //result = "";
    if (args.Count()==3)
	sep = (char *)args.GetString(2);
    else if (args.Count()>3)
    {
	result = "Bad # of args in split";
	return ERROR;
    }
    if (sep && sep[0]) for (;;)
    {
	char *start = p;
	while (*p && !strchr(sep, *p)) p++;
	int oldch = *p;
	*p = 0;
	AddListElt(result, start);
	if (oldch)
	    *p++ = oldch;
	else break;
    }
    else while (*p)
    {
	String tmp;
	tmp = (char)(*p++);
	AddListElt(result, tmp);
    }
    return OKAY;
}

int joinH(StringGroup &args, String &result)
{
    TraceThis();
    result = "";
    StringGroup elts;
    char *sep = " ";
    int ne = SplitList(args.GetString(1), elts, 0);
    if (args.Count()==3)
	sep = args.GetString(2);
    else if (args.Count()==3)
    {
        result = "wrong # args in join";
	return ERROR;
    }
    for (int i = 0; i < ne; i++)
    {
	if (i) result += sep;
	result += elts.GetString(i);
    }
    return OKAY;
}

int listH(StringGroup &args, String &result)
{
    TraceThis();
    result = "";
    for (int i = 1; i < args.Count(); i++)
	AddListElt(result, args.GetString(i), " ", 1);
    return OKAY;
}

int lrangeH(StringGroup &args, String &result)
{
    TraceThis();
    result = "";
    StringGroup elts;
    int ne = SplitList(args.GetString(1), elts, 0);
    int first, last;
    if (!args.GetString(2).IsNumeric(&first))
    {
        result = "bad `first' arg in lrange";
	return ERROR;
    }
    else if (strcmp((char *)args.GetString(3), "end")==0)
	last = ne-1;
    else if (!args.GetString(3).IsNumeric(&last))
    {
        result = "bad `last' arg in lrange";
	return ERROR;
    }
    else if (last>=ne) last = ne-1;
    for (int i = first; i <= last; i++)
	AddListElt(result, elts.GetString(i));
    return OKAY;
}

int lreplaceH(StringGroup &args, String &result)
{
    TraceThis();
    result = "";
    StringGroup elts;
    int ne = SplitList(args.GetString(1), elts, 0);
    int first, last;
    if (!args.GetString(2).IsNumeric(&first))
    {
        result = "bad `first' arg in lrange";
	return ERROR;
    }
    else if (strcmp((char *)args.GetString(3), "end")==0)
	last = ne-1;
    else if (!args.GetString(3).IsNumeric(&last))
    {
        result = "bad `last' arg in lrange";
	return ERROR;
    }
    for (int i = 0; i < first; i++)
	AddListElt(result, elts.GetString(i));
    for (int i = 4; i < args.Count(); i++)
	AddListElt(result, args.GetString(i));
    for (int i = last+1; i < ne; i++)
	AddListElt(result, elts.GetString(i));
    return OKAY;
}

int lsearchH(StringGroup &args, String &result)
{
    TraceThis();
    int na = args.Count(), a = 1, pattype = 1;
    while (a<na)
    {
	char *opt = args.GetString(a);
	if (opt[0]=='-')
	{
	    if (strcmp(opt, "-exact")==0)
		pattype = 0;
	    else if (strcmp(opt, "-glob")==0)
		pattype = 1;
	    else if (strcmp(opt, "-regexp")==0)
		pattype = 2;
	    else
	    {
	        result = "bad option in lsearch";
		return ERROR;
	    }
	    a++;
	}
	else break;
    }
    if (a != (na-2))
    {
        result = "bad # of args in lsearch";
	return ERROR;
    }
    char *rerr, *pat = args.GetString(a+1);
    StringGroup elts;
    int ne = SplitList(args.GetString(a), elts, 0), e;
    switch(pattype)
    {
    case 0: // exact
	for (e = 0; e < ne; e++)
	{
	    if (strcmp(pat, (char *)elts.GetString(e))==0)
	    {
		result = e;
		return OKAY;
	    }
	}
	break;
    case 1: // glob
	if ((rerr = SetGlobPattern(pat)) != NULL)
	    goto badpat;
	for (e = 0; e < ne; e++)
	{
	    if (GlobMatch((char *)elts.GetString(e)))
	    {
		result = e;
		return OKAY;
	    }
	}
	break;
    case 2: // regexp
	if ((rerr = SetRegexpPattern(pat)) != NULL)
	    goto badpat;
	for (e = 0; e < ne; e++)
	{
	    if (RegexpMatch((char *)elts.GetString(e)))
	    {
		result = e;
		return OKAY;
	    }
	}
	break;
    }
    result = -1;
    return OKAY;
badpat:
    result = "Bad pattern in lsearch: ";
    result += rerr;
    return ERROR;
}

int lsortH(StringGroup &args, String &result)
{
    TraceThis();
    int key = ASCII_KEY, increasing = 1, a = 1, na = args.Count();
    char *compare=NULL;
    while (a < na)
    {
	char *opt = args.GetString(a);
	if (opt[0]=='-')
	{
	    if (strcmp(opt, "-ascii")==0)
		key = ASCII_KEY;
	    else if (strcmp(opt, "-integer")==0)
		key = INTEGER_KEY;
	    else if (strcmp(opt, "-real")==0)
		key = REAL_KEY;
	    else if (strcmp(opt, "-command")==0)
	    {
		key = COMMAND_KEY;
		a++;
		if (a==na)
	        {
	            result = "bad -command option in lsort";
		    return ERROR;
	        }
		compare = args.GetString(a);
	    }
	    else if (strcmp(opt, "-increasing")==0)
		increasing = 1;
	    else if (strcmp(opt, "-decreasing")==0)
		increasing = 0;
	    else
	    {
	        result = "bad option in lsort";
		return ERROR;
	    }
	    a++;
	}
	else break;
    }
    if (a != (na-1))
    {
        result = "bad number of arguments in lsort";
        return ERROR;
    }
    SortableStringGroup elts;
    int ne = SplitList(args.GetString(a), elts, 0);
    elts.Sort(key, increasing, compare);
    for (int i = 0; i < ne; i++)
	AddListElt(result, elts.GetString(i));
    return OKAY;
badargs:
    result = "bad options or arguments to lsort";
    return ERROR;
}

int FormatString(StringGroup &args, String &res, int first)
{
    char format[32], *f, buff[256];
    char *fmt = args.GetString(first);
    int a = first+1, na = args.Count();
    res = "";
    while (*fmt)
    {
	if (*fmt == '%')
	{
	    f = fmt;
	    fmt++;
	    if (*fmt=='%')
	    {
		res += (char)'%';
		fmt++;
		continue;
	    }
	    int islong = 0;
	    while (*fmt)
	    {
		if (*fmt == 'l' && !islong)
		    islong = 1;
		else if (isalpha(*fmt))
		    break;
		fmt++;
	    }
	    if (*fmt==0) return -1;
	    else fmt++;
	    strncpy(format, f, fmt-f);
	    format[fmt-f] = 0;
	    char typ = format[fmt-f-1];
	    if (a >= na) return -1;
	    char *aptr = (char *)args.GetString(a++);
	    switch (typ)
	    {
	    case 'd':
	    case 'i':
	    case 'o':
	    case 'u':
	    case 'x':
	    case 'X':
		if (islong) sprintf(buff, format, atol(aptr));
		else sprintf(buff, format, atoi(aptr));
		res += buff;
		break;
	    case 'f':
		if (islong) return -1; // unsupported
		sprintf(buff, format, atof(aptr));
		res += buff;
		break;
	    case 's':
		if (islong) return -1; // invalid
		// NB - the next is dangerous!! We could overrun...
		sprintf(buff, format, aptr);
		res += buff;
		break;
	    case 'c':
		if (islong) return -1; // invalid
		sprintf(buff, format, (char)(atoi(aptr)));
		res += buff;
		break;
	    }
	}
	else res += (char)(*fmt++);
    }
    return 0;
}

// very basic at present

int ScanString(String &fmtstr, String &input, StringGroup &res)
{
    char format[32], *f, buff[256];
    char *fmt = fmtstr, *inp = input;
    int a = 0;
    while (*fmt)
    {
	if (*fmt == '%')
	{
	    f = fmt;
	    fmt++;
	    if (*fmt=='%')
	    {
		if (*inp != '%') return a;
		inp++;
		fmt++;
		continue;
	    }
	    int islong = 0;
	    while (*fmt)
	    {
		if (*fmt == 'l' && !islong)
		    islong = 1;
		else if (isalpha(*fmt))
		    break;
		fmt++;
	    }
	    if (*fmt==0) return -1;
	    else fmt++;
	    strncpy(format, f, fmt-f);
	    format[fmt-f] = 0;
	    char typ = format[fmt-f-1];
	    char *b = buff;
	    switch (typ)
	    {
	    case 'u':
	    case 'i':
	    case 'd':
		while (isdigit(*inp))
		    *b++ = *inp++;
		*b = 0;
		if (b == buff) return -1;
		break;
	    case 's':
		while (isspace(*inp)) inp++;
		while (*inp && !isspace(*inp))
		    *b++ = *inp++;
		*b = 0;
		if (b == buff) return -1;
		break;
	    case 'c':
		if (*inp == 0) return -1;
		if ((*inp)>99) *b++ = ((*inp)/100)+'0';
		if ((*inp)>9) *b++ = (((*inp)%100)/10)+'0';
		*b++ = ((*inp++)%10)+'0';
		*b = 0;
		break;
	    default:
		return -1;
	    }
	    res.SetString(a++, buff);
	}
	else if (*inp != *fmt)
	    if (*inp == ' ') *inp++;
	    else if (*fmt == ' ') *fmt++;
	    else break;
	else { fmt++; inp++; }
    }
    return a;
}


