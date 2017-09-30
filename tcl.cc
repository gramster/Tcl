#include "tcl.h"

FILE *debug = NULL;
FILE *input = stdin;
int profile = 0;
static int exitcode = 0;

String errorInfo;
String scriptfile;
static unsigned long cmdcount = 0l;

char *enm[] = {
	"OKAY", "ERROR", "RETURN", "BREAK", "CONT"
};

typedef struct
{
    prochandler_t fn;
    int nargs;
} dispatch_entry_t;

// handlers in lists.cpp

extern int lindexH(StringGroup &args, String &result);
extern int llengthH(StringGroup &args, String &result);
extern int linsertH(StringGroup &args, String &result);
extern int lappendH(StringGroup &args, String &result);
extern int concatH(StringGroup &args, String &result);
extern int splitH(StringGroup &args, String &result);
extern int joinH(StringGroup &args, String &result);
extern int listH(StringGroup &args, String &result);
extern int lrangeH(StringGroup &args, String &result);
extern int lreplaceH(StringGroup &args, String &result);
extern int lsearchH(StringGroup &args, String &result);
extern int lsortH(StringGroup &args, String &result);

// handlers in file.cpp

extern int cdH(StringGroup &args, String &result);
extern int closeH(StringGroup &args, String &result);
extern int eofH(StringGroup &args, String &result);
extern int fileH(StringGroup &args, String &result);
extern int flushH(StringGroup &args, String &result);
extern int getsH(StringGroup &args, String &result);
extern int openH(StringGroup &args, String &result);
extern int putsH(StringGroup &args, String &result);
extern int globH(StringGroup &args, String &result);

extern int stringH(StringGroup &args, String &result);
extern int arrayH(StringGroup &args, String &result);

#if __MSDOS__
extern "C" {
extern unsigned long far hrtime(void);
extern void far	hrt_open(void);
extern void far	hrt_close(void);
}
#endif

static int tcnt = 0;

static unsigned long StartTimer()
{
#if __MSDOS__
    if (tcnt++ == 0) hrt_open();
    return hrtime();
#else
    return 0l; // for now
#endif
}

static unsigned long StopTimer(unsigned long starttime)
{
#if __MSDOS__
    starttime = hrtime() - starttime;
    if (--tcnt == 0) hrt_close();
    if (starttime > 390451572l) starttime = 390451572l;
    // convert and round to nearest millisecond
    return (starttime*11l + 6562l) / 13125l;
#else
    return 0l; // 4 now
#endif
}

// useful utility for searching string tables

int FindString(char *nm, char *tbl[], int tblsz)
{
    TraceThis();
    int l = 0, r = (tblsz-1), x;
    while (r >= l)
    {
	x = (l+r)/2;
    	int cmp = strcmp(tbl[x], nm);
	if (cmp == 0)
	    return x;
	else if (cmp>0) r = x-1;
	else l = x+1;
    }
    return -1;
}

static void ReadFile(FILE *fp, int numbytes, int delnl, String &result)
{
    String input;
    while (!feof(fp) && numbytes != 0)
    {
        char buff[512];
        fgets(buff, 512, fp);
        int cnt = strlen(buff);
        if (numbytes > cnt) numbytes -= cnt;
        else if (numbytes > 0) numbytes = 0;
        input += buff;
    }
    if (delnl && input[input.Length()-1]=='\n')
        input.Trunc(input.Length()-1);
    result = input;
}

#if __MSDOS__

/* findCommandType, given a command, returns one of the following:

		0 - unknown type
		1 - internal DOS command
		2 - DOS batch file
		3 - DOS .EXE
		4 - DOS .COM

	If the command has an extension, this is checked, and a value
	returned. Otherwise, if the file has a backslash in its name,
	we check for a .BAT, .EXE or .COM file of that name and path.
	If no backslash, we check the current directory and then the
	path.
*/

static char *DOSexts[] = { ".BAT", ".EXE", ".COM", NULL };

int findCommandType(char *cmd)
{
    (void)cmd; return 0; // for now
}

static void runCommand(char *cmd, int Catch, String &result)
{
    int newf = -1, fail = 0, oldstderr=0, oldstdout=0;
    int i, len = strlen(cmd);
    int argc = 0, argbuf;
    char *(argv[64]);
    int DOSerr;
    String buf;

    /* If the command contains pipes or < or >, then write it to
    		a scratch .BAT file and make this the command */

    for (i = 0; cmd[i]; i++)
    {
    	if (cmd[i] == '>' || cmd[i]=='<' || cmd[i] == '|')
    	{
    	    /* Yes, we must make a batch file */
	    String batch("\\gc4.bat");
	    FILE *batfp;
    	    if ((batfp = fopen("\\gc4.bat","w")) != NULL)
    	    {
    	    	fputs(cmd,batfp);
    	    	fclose(batfp);
    	    }
	    argv[0] = "command";
	    argv[1] = "/c";
	    argv[2] = "\\gc4.bat";
    	    argv[3] = NULL;
	    argc = 4;
    	    break;
    	}
    }
    if (cmd[i] == 0)
    {
    	/* split the command up into an argv[] array of args */
    	buf = cmd;
    	for (i = 0;;)
    	{
    	    while (isspace(cmd[i])) i++;
    	    if (i>=len) break;
    	    argv[argc++] = ((char *)buf) + i;
    	    if (argc == 64) break;
    	    while (i<len && !isspace(cmd[i])) i++;
    	    if (i<len) buf[i++] = '\0';
    	    if (argc==1 && findCommandType(argv[0])<3)
    	    {
    	    	/* Not a .COM or .EXE - slap a shell on the front */
    	    	argv[2] = argv[0];
    	    	argv[0] = "command";
    	    	argv[1] = "/c";
    	    	argc = 3;
    	    }
    	}
    	argv[argc] = NULL;
    }

    /* Prepare the temp file for catching output */

    if (Catch)
    {
    	/* Redirect stderr to stdout, and stdout to the temp file. */
    	oldstdout = dup(1);
    	oldstderr = dup(2);
    	close(1); close(2);
    	newf = open("\\gc4catch.tmp",O_CREAT,S_IWRITE|S_IREAD);
    	if (newf>0)
    	    (void)dup(newf); /* dup stdout as stderr */
    	else
    	{
    	    /* undo */
    	    (void)dup(oldstdout);
	    (void)dup(oldstderr);
    	    close(oldstdout);
	    close(oldstderr);
    	}
//    	showMsg(cmd);
    }
    else
    {
    	/* DOS wants a screen clear; else you don't see any output! */
    	clrscr();
    	fprintf(stderr,"%s\n",cmd); /* show the command */
    }
    if (spawnvp(P_WAIT,argv[0],argv)!=0)
    {
    	fail = errno;
    	DOSerr = _doserrno;
    }
    unlink("\\gc4.bat");
//  if (!Catch) (void)saveScreen();
    if (Catch && newf > 0)
    {
    	/* restore the files */
    	close(1); close(2);
    	(void)dup(oldstdout);	(void)dup(oldstderr);
    	close(oldstdout);	close(oldstderr);
    }
    if (fail)
    {
    	int len;
    	result = strerror(fail);
    	result += "(DOS err: ";
    	result += strerror(DOSerr);
        result.Trunc(result.Length()-1);
    	result += ")";
    }
    if (Catch)
    {
    	FILE *catchFP = fopen("\\gc4catch.tmp","r");
	if (fp) ReadFile(catchFP, -1, 0, result);
    	unlink("\\gc4catch.tmp");
    }
}

#else

/* Old version for UNIX */

static void runCommand(char *cmd, int katch, String &result)
{
    char *msg;
    int fail = 0;
    String buf(cmd), tmp(tmpnam(NULL));
    if (katch)
        buf += " > " + tmp + " 2>&1 ";
    if (system(msg)) fail = errno;
    if (katch)
    {
    	FILE *catchFP = fopen((char *)tmp,"r");
	if (catchFP) ReadFile(catchFP, -1, 0, result);
    	unlink((char *)tmp);
    }
#ifndef NO_STRERROR
    if (fail)
    	result = strerror(fail);
#endif
}

#endif

char *SplitVar(String v, String* &base, String* &idx)
{
    TraceThis();
    char *b = (char *)v, *i = NULL;
    char *tp = strchr(b, '(');
    if (tp)
    {
	*tp++ = 0;
        i = tp;
	tp = strchr(tp, ')');
	if (tp) *tp = 0;
    }
    if (b) base = new String(b);
    else base = NULL;
    if (i && *i) idx = new String(i);
    else idx = NULL;
    return idx ? ((char *)*idx) : NULL;
}

static char *CalcExpr(String e)
{
    TraceThis();
    static char rs[16];
    int val;
    char *msg = compute(e, &val);
    if (msg)
	return msg;
    else
    {
	sprintf(rs, "%d", val);
	return rs;
    }
}

int appendH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    String *base, *idx;
    char *id = SplitVar(args.GetString(1), base, idx);
    result = GetVar((char *)*base, id);
    for (int j = 2; j < args.Count(); j++)
    {
        result += (char)' ';
        result += args.GetString(j);
    }
    SetVar(result, (char *)*base, id);
    if (base) delete base;
    if (idx) delete idx;
    return rtn;
}

int whileH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    for (;;)
    {
	int ev;
    	String val = CalcExpr(args.GetString(1));
	if (!val.IsNumeric(&ev))
	{
	    result = val;
	    rtn = ERROR;
	    break;
	}
	else if (ev==0)
	    break;
	else
	{
    	    rtn = Interpret(&args.GetString(2), result);
	    if (rtn == CONT) rtn = OKAY; 
	    else if (rtn != OKAY)
	    {
	        if (rtn == BREAK) rtn = OKAY;
	        break;
	    }
	}
    }
    return rtn;
}

int forH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    rtn = Interpret(&args.GetString(1), result);
    if (rtn==OKAY)
    {
        for (;;)
        {
	    int ev;
    	    String val = CalcExpr(args.GetString(2));
    	    if (!val.IsNumeric(&ev))
    	    {
		result = val;
		rtn = ERROR;
		break;
	    }
	    else if (ev==0)
	        break;
	    else
	    {	    
    	        rtn = Interpret(&args.GetString(4), result);
	        if (rtn == CONT) rtn = OKAY; 
	        else if (rtn != OKAY)
	        {
	     	    if (rtn == BREAK) rtn = OKAY;
		    break;
		}
	    }
	    {	    
	        if ((rtn = Interpret(&args.GetString(3), result)) != OKAY)
	    	    break;
	    }
        }
    }
    return rtn;
}

int ifH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    int a = 1;
    for (;;)
    {
	if ((a+1)>=args.Count())
	{
    	    result = "Bad if statement";
	    rtn = ERROR;
	    break;
	}
	else
	{
	    int ev;
    	    String val = CalcExpr(args.GetString(a));
	    if (!val.IsNumeric(&ev))
	    {
		result = val;
	    	rtn = ERROR;
		break;
	    }
            else if (ev!=0)
	    {
    	    	rtn = Interpret(&args.GetString(a+1), result);
	    	break;
	    }
	}
	a += 2;
	if (a >= args.Count())
	    break;
	if (strcmp((char *)args.GetString(a), "else")==0)
	{
	    if ((a+2)!=args.Count())
	    {
    	        result = "Bad else in if statement";
		rtn = ERROR;
	    }
	    else
    	        rtn = Interpret(&args.GetString(a+1), result);
	    break;
	}
	if (strcmp((char *)args.GetString(a), "elseif")!=0)
	{
    	    result = "elseif expected in if statement (got ";
    	    result += args.GetString(a) + ")";
	    rtn = ERROR;
	    break;
	}
	else a++;
    }
    return rtn;
}

int setH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    if (args.Count() != 2 && args.Count()!=3)
    {
        result = "wrong # args in set";
	rtn = ERROR;
    }
    else
    {
	String *base, *idx;
	char *i = SplitVar(args.GetString(1), base, idx);
	if (args.Count() == 3)
 	    SetVar(args.GetString(2), (char *)*base, i);
	char *v = GetVar((char *)*base, i);
	if (base) delete base;
	if (idx) delete idx;
	if (v)
	    result = v;
	else
	{
            result = "Undefined variable ";
            result += args.GetString(1);
	    rtn = ERROR;
	}
    }
    return rtn;
}

int delta(StringGroup &args, char *nm, int sgn, String &result)
{
    TraceThis();
    int rtn = OKAY;
    int dt = 1;
    if (args.Count() != 2 && args.Count() != 3)
    {
        result = "wrong # args in ";
        result += nm;
	rtn = ERROR;
    }
    else if (args.Count()==3 && !args.GetString(2).IsNumeric(&dt))
    {
        result = "Expected value in ";
        result += nm;
        rtn = ERROR;
    }
    else
    {
	String *base, *idx;
	char *i = SplitVar(args.GetString(1), base, idx);
	char *vp = GetVar((char *)*base, i);
	if (vp==NULL)
	{
	    result = "Undefined variable ";
	    result += args.GetString(1);
	    rtn = ERROR;
	}
	else
	{
	    int v = atoi(GetVar((char *)*base, i));
	    char s[20];
	    sprintf(s, "%d", v+dt*sgn);
	    SetVar(s, (char *)*base, i);
	    vp = GetVar((char *)*base, i);
	    assert(vp);
	    result = vp;
	}
	if (base) delete base;
	if (idx) delete idx;
    }
    return rtn;
}

int incrH(StringGroup &args, String &result)
{
    TraceThis();
    return delta(args, "incr", 1, result);
}

int decrH(StringGroup &args, String &result)
{
    TraceThis();
    return delta(args, "decr", -1, result);
}

int errorH(StringGroup &args, String &result)
{
    TraceThis();
    result = args.GetString(1);
    if (args.Count()>2)
    {
	errorInfo = args.GetString(2);
        if (args.Count()>3) // got errorcode or error; we don't do code
            result = "wrong # args in upvar";
    }
    return ERROR;
}

int upvarH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    int lvl = 1;
    if (args.Count()!=3 && args.Count()!=4)
    {
        result = "wrong # args in upvar";
	rtn = ERROR;
    }
    else
    {
	if (args.Count() == 4)
	{
	    if (!args.GetString(1).IsNumeric(&lvl))
	    {
	    	result = "invalid level argument";
		rtn = ERROR;
	    }
	}
    }
    if (rtn == 0)
    {
	String *lbase, *lidx, *gbase, *gidx;
	char *li = SplitVar(args.GetString(args.Count()-1), lbase, lidx);
	char *gi = SplitVar(args.GetString(args.Count()-2), gbase, gidx);
	if (MakeAlias(lvl, (char *)*gbase, gi, (char *)*lbase, li) != 0)
	{
	    result = "upvar failed";
	    rtn = ERROR;
	}
	if (gbase) delete gbase;
	if (gidx) delete gidx;
	if (lbase) delete lbase;
	if (lidx) delete lidx;
    }
    return rtn;
}

int uplevelH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    int lvl, first=1;
    if (args.Count()>1 && args.GetString(1).IsNumeric(&lvl))
	first = 2;
    else lvl = 1;
    String e;
    for (int i = first; i < args.Count(); i++)
    {
	e += args.GetString(i);
	e += (char)' ';
    }
    VarStack tmp;
    UpLevel(tmp, lvl);
    rtn = Interpret(&e, result);
    DownLevel(tmp);
    return rtn;
}

int globalH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    for (int i = 1; i < args.Count(); i++)
    {
	String *base, *idx;
	char *index = SplitVar(args.GetString(i), base, idx);
    	if (MakeAlias(-1, (char *)*base, index, (char *)*base, index) != 0)
	{
	    result = "global failed";
	    rtn = ERROR;
	}
	if (base) delete base;
	if (idx) delete idx;
	if (rtn) break;
    }    
    return rtn;
}

int traceH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY, na = 5, op=-1;
    char *a1 = args.GetString(1);
    if (strcmp(a1,"variable")==0) op = 0;
    else if (strcmp(a1,"vdelete")==0) op = 1;
    else if (strcmp(a1,"vinfo")==0) { op = 2; na = 3; }
    if (args.Count()!=na)
    {
        result = "wrong # args in trace";
	rtn = ERROR;
    }
    else
    {
	String *base, *idx;
	char *index = SplitVar(args.GetString(2), base, idx);
	char *fl = args.GetString(3); // ignored flags for now
	unsigned f = 0;
	while (*fl)
	{
	    if (*fl =='w') f |= TRACE_SET;
	    else if (*fl =='r') f |= TRACE_GET;
	    else if (*fl =='u') f |= TRACE_UNSET;
	    else
	    {
	        result = "bad flags in trace";
		rtn = ERROR;
		break;
	    }
	    fl++;
	}
	if (f && rtn==OKAY) switch(op)
	{
	case 0:
	    AddTrace((char *)*base, index, f, args.GetString(4));
	    break;
	case 1:
	    DelTrace((char *)*base, index, f, args.GetString(4));
	    break;
	case 2:
	    TraceInfo((char *)*base, index, result);
	    break;
	}
	if (base) delete base;
	if (idx) delete idx;
    }
    return rtn;
}

int unsetH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    for (int i = 1; i < args.Count() && rtn==OKAY; i++)
    {
	String *base, *idx;
	char *index = SplitVar(args.GetString(i), base, idx);
	if (DestroyVar((char *)*base, index) != 0)
	{
	    result = "Invalid or undefined variable ";
	    result += args.GetString(i);
	    rtn = ERROR;
	}
	if (base) delete base;
	if (idx) delete idx;
    }    
    return rtn;
}

int evalH(StringGroup &args, String &result)
{
    TraceThis();
    String e;
    for (int i = 1; i < args.Count(); i++)
    {
	e += args.GetString(i);
	e += (char)' ';
    }    
    return Interpret(&e, result);
}

int timeH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY, lim;
    if (!args.GetString(2).IsNumeric(&lim))
    {
        result = "numeric arg expected in time";
	rtn = ERROR;
    }
    else
    {
	int cnt = lim;
	unsigned long tm = StartTimer();
	while (lim-- > 0)
	{
	    if ((rtn = Interpret(&args.GetString(1), result)) != OKAY)
		break;
	}
	if (rtn == OKAY)
	{
	    result = (int)(StopTimer(tm)/cnt);
	    result += " milliseconds per iteration";
	}
    }
    return rtn;
}

int catchH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn;
    if (args.Count()!=2 && args.Count()!=3)
    {
        result = "wrong # args in catch";
	rtn = ERROR;
    }
    else
    {
	rtn = Interpret(&args.GetString(1), result);
	if (args.Count() == 3)
	    SetVar(result, args.GetString(2));
	if (debug)
	    fprintf(debug, "Caught %s!\n", enm[rtn]);
	result = rtn;
	rtn = OKAY;
    }
    errorInfo = "";
    return rtn;
}

int exprH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY, ei;
    String e;
    for (int i = 1; i < args.Count(); i++)
	e += args.GetString(i);
    result = CalcExpr(e);
    if (!result.IsNumeric(&ei))
	rtn = ERROR;
    return rtn;
}

int sourceH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    FILE *oldinput = input;
    input = fopen(args.GetString(1), "r");
    if (input == NULL)
    {
        input = oldinput;
        result = "Cannot open ";
        result += args.GetString(1);
        rtn = ERROR;
    }
    else
    {
	String thisscript = scriptfile;
	scriptfile = args.GetString(1);
        rtn = Interpret(NULL, result);
	scriptfile = thisscript;
        fclose(input);
        if (rtn==RETURN) rtn = OKAY;
    }
    input = oldinput;
    return rtn;
}

int exitH(StringGroup &args, String &result)
{
    TraceThis();
    if (args.Count()==2 && !args.GetString(1).IsNumeric(&exitcode))
    {
	exitcode = -1;
	result = "Invalid exit code in statement";
    }
    return -1;
}

int procH(StringGroup &args, String &result)
{
    TraceThis();
    DefineProc(args.GetString(1), args.GetString(2), args.GetString(3));
    result = "";
    return OKAY;
}

int foreachH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn=OKAY;
    StringGroup elts;
    int ne = SplitList(args.GetString(2), elts, 0);
    for (int i = 0; i < ne; i++)
    {
        SetVar(elts.GetString(i), args.GetString(1));
        rtn = Interpret(&args.GetString(3), result);
        if (rtn == CONT) rtn = OKAY;
        else if (rtn != OKAY) break;
    }    
    if (rtn == BREAK) rtn = OKAY;
    return rtn;
}

int returnH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = RETURN, a = 1, rtnv;
    int na = args.Count();
    while (a<na)
    {
	char *opt = args.GetString(a);
	if (opt[0]=='-')
	{
	    if (strcmp(opt, "-code")==0)
	    {
		if (a==(na-1)) goto badargs;
		char *opt2 = args.GetString(a+1);
		if (isdigit(*opt2))
		{
		    if (!args.GetString(a+1).IsNumeric(&rtn))
		    {
		    	result = "Bad code argument in return";
		   	return ERROR;
		    }
		}
		else if (strcmp(opt2, "ok")==0)
		    rtn = OKAY;
		else if (strcmp(opt2, "error")==0)
		    rtn = ERROR;
		else if (strcmp(opt2, "return")==0)
		    rtn = RETURN;
		else if (strcmp(opt2, "break")==0)
		    rtn = BREAK;
		else if (strcmp(opt2, "continue")==0)
		    rtn = CONT;
		else
		{
		    result = "Bad code argument in return";
		    return ERROR;
		}
	    }
	    else if (strcmp(opt, "-errorinfo")==0)
	    {
		if (a==(na-1)) goto badargs;
		errorInfo = args.GetString(a+1);
	    }
	    else
	    {
		result = "Bad option ";
		result += opt;
		result += " in return";
		return ERROR;
	    }
	}
	else if (a == (na-1))
	    result = args.GetString(a);
	else goto badargs;
	a+=2;
    }
    return rtn;
badargs:
    result = "wrong # or bad args in return";
    errorInfo = "";
    return ERROR;
}

int continueH(StringGroup &args, String &result)
{
    TraceThis();
    (void)args; result = "";
    return CONT;
}

int breakH(StringGroup &args, String &result)
{
    TraceThis();
    (void)args; result = "";
    return BREAK;
}

int renameH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    ProcVariable *pv = LookupProc(args.GetString(1));
    if (pv == NULL)
    {
        result = "Undefined procedure ";
        result += args.GetString(1);
        rtn = ERROR;
    }
    else if (args.Count()==2)
	RemoveProc(args.GetString(1));
    else if (args.Count()==3)
	RenameProc(args.GetString(1), args.GetString(2));
    else
    {
        result = "Wrong number of args in rename";
        rtn = ERROR;
    }
    return rtn;
}

int formatH(StringGroup &args, String &result)
{
    FormatString(args, result, 1);
    return OKAY;
}

static char *info_opts[] =
{
    "args",		// 0
    "body",
    "cmdcount",
    "commands",
    "default",
    "exists",		// 5
    "gcversion",
    "globals",
    "level",
    "library",
    "locals",		// 10
    "procs",
    "script",
    "vars"
};

int FindDefault(char *name, char *argname, String &res)
{
    UserProcVariable *pv = (UserProcVariable *)LookupProc(name);
    if (pv == NULL || pv->IsInternal()) return 0;
    StringGroup params;
    int na = SplitList(pv->Args(), params, 0);
    for (int a = 0; a < na; a++)
    {
        char *name, *defval;
        StringGroup tmp;
        int lc = SplitList(params.GetString(a), tmp, 3);
        if (lc==2 && strcmp(argname, (char *)tmp.GetString(0))==0)
	{
	    res = tmp.GetString(1);
	    return 1;
	}
    }
    return 0;
}

int infoH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY, na = args.Count();
    int opt = (na < 2 ) ? -1 :
		FindString(args.GetString(1), info_opts, 
			sizeof(info_opts)/sizeof(info_opts[0]));
    ProcVariable *pv;
    switch (opt)
    {
    default:
        result = "Bad option arg in info";
	rtn = ERROR;
	break;
    case 0: // args
	if (na != 3) goto badargs;
	pv = LookupProc((char *)args.GetString(2));
	if (pv && !pv->IsInternal())
	    result = ((UserProcVariable*)pv)->Args();
	break;
    case 1: // body
	if (na != 3) goto badargs;
	pv = LookupProc((char *)args.GetString(2));
	if (pv && !pv->IsInternal())
	    result = ((UserProcVariable*)pv)->Body();
	break;
    case 2: // cmdcount
	result = (int)cmdcount;
	break;
    case 3: // commands
	if (na>3) goto badargs;
	else if (na==3)
	    GetCommands(result, (char *)args.GetString(2));
	else
	    GetCommands(result);
	break;
    case 4: // default
	if (na != 5) goto badargs;
	if (FindDefault((char *)args.GetString(2), (char *)args.GetString(3),
		result))
	{
	    String *base, *idx;
	    char *i = SplitVar(args.GetString(4), base, idx);
	    SetVar(result, (char *)*base, i);
	    if (base) delete base;
	    if (idx) delete idx;
	    result = "1";
	}
	else result = "0";
	break;
    case 5: // exists
	if (na != 3) goto badargs;
	else
        {
	    String *base, *idx;
	    char *i = SplitVar(args.GetString(2), base, idx);
	    result = (int)(GetVar((char *)*base, i) != NULL);
	    if (base) delete base;
	    if (idx) delete idx;
	}
	break;
    case 6: // gcversion
	result = "4.0"; // for now
	break;
    case 7: // globals
	if (na>3) goto badargs;
	else if (na==3)
	    GetGlobals(result, (char *)args.GetString(2));
	else
	    GetGlobals(result);
	break;
    case 8: // level
	if (na == 3)
	{
	    int lvl;
	    if (!args.GetString(2).IsNumeric(&lvl))
	    {
		result = "Bad level number in info level";
		rtn = ERROR;
	    }
	    else GetLevelInfo(result, lvl);
	}
	else GetLevelInfo(result);
	break;
    case 9: // library
	break;
    case 10: // locals
	if (na>3) goto badargs;
	else if (na==3)
	    GetLocals(result, (char *)args.GetString(2));
	else
	    GetLocals(result);
	break;
    case 11: // procs
	if (na>3) goto badargs;
	else if (na==3)
	    GetProcs(result, (char *)args.GetString(2));
	else
	    GetProcs(result);
	break;
    case 12: // script
	result = scriptfile;
	break;
    case 13: // vars
	if (na>3) goto badargs;
	else if (na==3)
	    GetVars(result, (char *)args.GetString(2));
	else
	    GetVars(result);
	break;
    }
    return rtn;
badargs:
    result = "Bad number of args in info command";
    return ERROR;
}

int scanH(StringGroup &args, String &result)
{
    TraceThis();
    int na = args.Count();
    StringGroup res;
    result = ScanString(args.GetString(2), args.GetString(1), res);
    for (int i = 0; i < res.Count(); i++)
    {
	String *base, *idx;
	if ((i+3)>=na) break;
	char *id = SplitVar(args.GetString(i+3), base, idx);
	SetVar(res.GetString(i), (char *)*base, id);
	if (base) delete base;
	if (idx) delete idx;
    }
    result = res.Count();
    return OKAY;
}

int switchH(StringGroup &args, String &result)
{
    TraceThis();
    StringGroup elts;
    int a = 1, na = args.Count(), matchtyp = 1;
    while (a < na)
    {
        char *opt = args.GetString(a);
	if (opt[0]=='-')
	{
	    // handle option
	    if (strcmp(opt, "-exact")==0) matchtyp = 0;
	    else if (strcmp(opt, "-glob")==0) matchtyp = 1;
	    else if (strcmp(opt, "-regexp")==0) matchtyp = 2;
	    else
	    {
		result = "Bad option in switch";
		return ERROR;
	    }
	}
	else break;
	a++;
    }
    if (a > (na-2))
    {
    	result = "Invalid # of args in switch";
	return ERROR;
    }
    char *string = args.GetString(a++);
    if (a == (na-1))
    	SplitList(args.GetString(a), elts, 0);
    else
    {
	int i = 0;
	while (a < na)
	    elts.SetString(i++, args.GetString(a++));
    }
    int matched = 0, ec = elts.Count();
    if (ec % 2)
    {
    	result = "Invalid # of pattern/actions in switch body";
	return ERROR;
    }
    for (a = 0; a < ec; a+=2)
    {
	if (!matched)
	{
	    if (a!=(ec-2) || strcmp((char *)elts.GetString(a), "default")!=0)
	    {
		char *pat = elts.GetString(a);
		if (matchtyp == 0)
		{
		    if (strcmp(pat, string)!=0)
			continue;
		}
		else if (matchtyp == 1)
		{
		    if (strcmp(pat, string)!=0)
			continue;
		}
		else if (matchtyp == 1)
		{
		    SetGlobPattern(pat);
		    if (GlobMatch(string)!=1)
			continue;
		}
		else
		{
		    SetRegexpPattern(pat);
		    if (RegexpMatch(string)!=1)
			continue;
		}
	    }
	}
	String &ip = elts.GetString(a+1);
	if (ip.Length()==1 && ip[0]=='-') matched = 1;
	else return Interpret(&ip, result);
    }
    return OKAY;
}

// only very basic support at present for regexp

int regexpH(StringGroup &args, String &result)
{
    TraceThis();
    int na = args.Count();
    if (na != 3)
    {
	result = "regexp only partially supported at present!";
	return ERROR;
    }
    SetRegexpPattern((char *)args.GetString(1));
    result = (int)(RegexpMatch( (char *)args.GetString(2)) );
    return OKAY;
}

int regsubH(StringGroup &args, String &result)
{
    TraceThis();
    (void)args; result = "";
    assert(0); // unsupported at present
    return OKAY;
}

int pwdH(StringGroup &args, String &result)
{
    TraceThis();
    (void)args;
#if __MSDOS__
    char buff[256];
    result = getcwd(buff, 256);
    return OKAY;
#else
    return OKAY; // for now.
#endif
}

int readH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    if (args.Count()<2 || args.Count()>3)
    {
        result = "wrong # args in read";
	rtn = ERROR;
    }
    else
    {
        int na = args.Count();
        int delnl = (strcmp((char *)args.GetString(1), "-nonewline")==0);
	int numbytes = -1;
        if (!delnl && na==3) // must have numbytes
        {
	    if (!args.GetString(2).IsNumeric(&numbytes))
	    {
                result = "bad bumber of bytes in read";
                rtn = ERROR;
	    }
	}
	if (rtn == OKAY)
	{
	    FILE *fp = FindFile(args.GetString(delnl ? 2 : 1));
	    if (fp == NULL)
	    {
                result = "bad file ID in read";
                rtn = ERROR;
	    }
	    else ReadFile(fp, numbytes, delnl, result);
        }
    }
    return rtn;
}

int seekH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    if (args.Count()<3 && args.Count()>4)
    {
        result = "wrong # args in seek";
	rtn = ERROR;
    }
    else
    {
	long offset;
        FILE *fp = FindFile(args.GetString(1));
        if (fp==NULL)
        {
            result = "bad file ID in tell";
            rtn = ERROR;
        }
        else if (!args.GetString(2).IsNumeric(&offset))
	{
            result = "file offset expected in seek";
            rtn = ERROR;
	}
	else 
	{
	    if (args.Count()==4)
	    {
		char *place = (char *)args.GetString(3);
		if (strcmp(place, "start") == 0)
		    fseek(fp, offset, SEEK_SET);
		else if (strcmp(place, "current") == 0)
		    fseek(fp, offset, SEEK_CUR);
		else if (strcmp(place, "end") == 0)
		    fseek(fp, offset, SEEK_END);
		else
		{
	            result = "start, current or end expected in seek";
        	    rtn = ERROR;
		}
	    }
	    else fseek(fp, offset, SEEK_SET);
	}
    }
    return rtn;
}

int tellH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    FILE *fp = FindFile(args.GetString(1));
    if (fp==NULL)
    {
        result = "bad file ID in tell";
        rtn = ERROR;
    }
    else
	result = (int)ftell(fp);
    return rtn;
}

//----------------------------------------------------------
// Process control 

int sleepH(StringGroup &args, String &result)
{
    int d;
    if (args.GetString(1).IsNumeric(&d))
    {
	sleep(d);
	return OKAY;
    }
    result = "Bad argument to sleep";
    return ERROR;
}

int execH(StringGroup &args, String &result)
{
    String e;
    for (int i = 1; i < args.Count(); i++)
    {
	e += args.GetString(i);
	e += (char)' ';
    }    
    runCommand(e, 0, result);
    return OKAY;
}

int execcatchH(StringGroup &args, String &result)
{
    String e;
    for (int i = 1; i < args.Count(); i++)
    {
	e += args.GetString(i);
	e += (char)' ';
    }    
    runCommand(e, 1, result);
    return OKAY;
}

static void SetupInternalCommands()
{
    // THESE SHOULD NOT BE ALPHABETICAL - WORST CASE FOR BINARY TREES!!

    DefineProc("linsert",   linsertH,  0 );
    DefineProc("exit",	    exitH,     1 );
    DefineProc("switch",    switchH,   0 );
    DefineProc("close",	    closeH,    2 );
    DefineProc("gets",	    getsH,    -2 );
    DefineProc("proc",	    procH,     4 );
    DefineProc("time",	    timeH,     3 );
    DefineProc("catch",	    catchH,    0 );
    DefineProc("incr",	    incrH,     0 );
    DefineProc("concat",    concatH,   0 );
    DefineProc("decr",	    decrH,     0 );
    DefineProc("break",	    breakH,    1 );
    DefineProc("info",	    infoH,    -2 );
    DefineProc("expr",	    exprH,    -1 );
    DefineProc("foreach",   foreachH,  4 );
    DefineProc("continue",  continueH, 1 );
    DefineProc("if",	    ifH,       0 );
    DefineProc("file",	    fileH,    -3 );
    DefineProc("eof",	    eofH,      2 );
    DefineProc("global",    globalH,  -1 );
    DefineProc("execcatch", execcatchH,2 );
    DefineProc("cd",	    cdH,       0 );
    DefineProc("join",	    joinH,     0 );
    DefineProc("flush",	    flushH,    1 );
    DefineProc("append",    appendH,  -3 );
    DefineProc("lindex",    lindexH,   3 );
    DefineProc("format",    formatH,  -2 );
    DefineProc("eval",	    evalH,    -1 );
    DefineProc("glob",	    globH,     0 );
    DefineProc("array",	    arrayH,   -3 );
    DefineProc("exec",	    execH,    -2 );
    DefineProc("for",	    forH,      5 );
    DefineProc("lappend",   lappendH, -3 );
    DefineProc("set",	    setH,      0 );
    DefineProc("lrange",    lrangeH,   4 );
    DefineProc("regsub",    regsubH,  -5 );
    DefineProc("tell",	    tellH,     2 );
    DefineProc("upvar",	    upvarH,    0 );
    DefineProc("puts",	    putsH,    -2 );
    DefineProc("llength",   llengthH,  2 );
    DefineProc("uplevel",   uplevelH,  0 );
    DefineProc("lsearch",   lsearchH, -3 );
    DefineProc("seek",	    seekH,    -3 );
    DefineProc("pwd",	    pwdH,      1 );
    DefineProc("list",	    listH,     0 );
    DefineProc("unset",	    unsetH,   -1 );
    DefineProc("open",	    openH,    -2 );
    DefineProc("string",    stringH,  -3 );
    DefineProc("rename",    renameH,  -2 );
    DefineProc("lreplace",  lreplaceH,-4 );
    DefineProc("read",	    readH,     0 );
    DefineProc("lsort",	    lsortH,   -2 );
    DefineProc("return",    returnH,   0 );
    DefineProc("scan",	    scanH,    -4 );
    DefineProc("regexp",    regexpH,   0 );

    DefineProc("sleep",	    sleepH,    2 );
    DefineProc("source",    sourceH,   2 );
    DefineProc("split",	    splitH,   -2 );
    DefineProc("trace",	    traceH,   -3 );
    DefineProc("while",	    whileH,    3 );
};

int CallUserProc(UserProcVariable *pv, StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    EnterFrame(&args);
    if (pv == NULL)
    {
        if ((pv = (UserProcVariable *)LookupProc("unknown")) != NULL
		&& !pv->IsInternal())
	{
	    String cmd("unknown");
	    for (int i = 0; i < args.Count(); i++)
	    {
		cmd += (char)' ';
		cmd += args.GetString(i);
	    }
	    rtn = Interpret(&cmd, result);
	}
	else
	{
	    result = "Undefined procedure ";
	    result += args.GetString(0);
	    rtn = ERROR;
	}
    }
    else
    {
        StringGroup params;
    	int na = SplitList(pv->Args(), params, 0);
	for (int a = 0; a < na; a++)
        {
	    char *name, *defval;
	    StringGroup tmp;
	    int lc = SplitList(params.GetString(a), tmp, 3);
	    if (lc==1)
	    {
	        name = (char *)params.GetString(a);
	        defval = NULL;
	    }
	    else if (lc==2)
	    {
	        name = (char *)tmp.GetString(0);
	        defval = (char *)tmp.GetString(1);
	    }
	    else
	    {
	        result = "Bad formal parameter in call to ";
		result += args.GetString(0);
	    	rtn = ERROR;
	    	break;
	    }
	    // Handle special arg "args"
	    if (a==(na-1) && strcmp(name, "args")==0)
	    {
	    	String rest;
	    	for (int j = a+1; j < args.Count(); j++)
		    AddListElt(rest, args.GetString(j));
	    	SetVar(rest, "args");
	    }
	    else if ((a+1)<args.Count()) // got actual param?
	    	SetVar(args.GetString(a+1), name);
	    else if (defval) // no actual; got default value?
	    	SetVar(defval, name);
	    else // no arg & no defalt
	    {
	    	result = "Missing parameter in call to ";
	    	result += args.GetString(0);
	    	rtn = ERROR;
	    	break;
	    }
        }
    	if (rtn == OKAY)
    	{
	    String b(pv->Body());
	    if (profile)
	    {
	    	unsigned long t = StartTimer();
	    	rtn = Interpret(&b, result);
	    	t = StopTimer(t);
	    	UpdateProfile((char *)args.GetString(0), t);
	    }	
	    else rtn = Interpret(&b, result);
        }
    }
    ExitFrame();
    if (rtn == RETURN)
	rtn = OKAY;
    else if (rtn == CONT || rtn == BREAK)
	rtn = ERROR;
    return rtn;
}

int Interpret(String *input, String &result)
{
    TraceThis();
    int rtn = OKAY;
    InputString *ipstr = NULL;
    int argcnt;
    int oldch = ch;
    if (input)
	ipstr = new InputString((char *)input->Text());
    result = "";
    if (debug)
	fprintf(debug, "In Interpret(%s)\nch = %d\n",
		input?input->Text():"file", ch);
    ch = '\n';
    for (;;)
    {
        StringGroup args;
	int p = ReadStatement(ipstr, args, result);
	assert(p<0 || args.Count()>0);
	if (debug)
	{
	    fprintf(debug, "ReadStatement returns %d\n", p);
	    if (p==0)
	    {
	        fprintf(debug, "EXECUTING \"");
	        for (int i = 0; i < args.Count(); i++)
	        {
	    	    if (i) fprintf(debug, " ");
	    	    fprintf(debug, "%s", (char *)args.GetString(i));
	        }
	        fprintf(debug, "\"\n");
	    }
	}
	if (p < 0)
	{
	    if (p<-1) rtn = ERROR; // no error on EOF
	    if (debug) fprintf(debug,"ReadStatement failed\n");
	}
	else
	{
//	    result="";
	    ProcVariable *pv = LookupProc((char *)args.GetString(0));
	    prochandler_t h = NULL;
	    if (pv && pv->IsInternal())
	    {
	        int na = ((InternalProcVariable*)pv)->NumArgs();
	        if (na && (na < 0 && args.Count() < -na) && na != args.Count())
	        {
		    result = "wrong # args in ";
		    result += args.GetString(0);
		    rtn = ERROR;
	        }
	        else h = ((InternalProcVariable*)pv)->Handler();
	    }
	    if (rtn == OKAY)
	    {
	        unsigned long t;
	        if (profile) t = StartTimer();
	        if (h)
	            rtn = (*h)(args, result);
	        else
	            rtn = CallUserProc((UserProcVariable*)pv, args, result);
	        if (profile)
	        {
	            t = StopTimer(t);
	            UpdateProfile((char *)args.GetString(0), t);
	        }
	        cmdcount++;
	        if (rtn==OKAY) continue;
	    }
	}
	if (rtn==ERROR)
	{
	    if (errorInfo.Length()==0)
		errorInfo = result + "\twhile executing\n";
	    else 
		errorInfo += "\tinvoked from within\n";
	    args.Dump(errorInfo);
	}
	else if (rtn == -1) // done exit?
	    rtn = ERROR;
        if (debug)
	{
	    if (rtn>=OKAY && rtn<=CONT)
	        fprintf(debug,"Interpret returns exception [%s]\n", enm[rtn]);
	    else 
	        fprintf(debug,"Interpret returns exception [USER %d]\n", rtn);
	}
	break;
    }
    if (ipstr) delete ipstr;
    ch = oldch;
    extern char discard;
    discard = 0;
    if (debug)
	fprintf(debug,"Interpreter result is %s\n", (char *)result);
    return rtn;
}

static void ProcessArgs(int argc, char *argv[])
{
    int i = 1;
    while (i < argc)
    {
	if (argv[i][0]=='-')
	{
	    switch(argv[i][1])
	    {
	    case 'D':
	    	debug = stderr;
	    	break;
	    case 'p':
	    	profile = 1;
	    	break;
	    default:
	    	fprintf(stderr, "Useage: tcl [-D] [-p] [<file>]\n");
	    	exit(0);
	    }
	}
	else if (input == stdin)
	{
	    input = fopen(argv[i], "r");
	    if (input==NULL)
	    {
	    	fputs("Can't open input", stderr);
	    	exit(-1);
	    }
	    scriptfile = argv[i];
	}
	i++;
    }
}

int RunInitScript(String &result)
{
    FILE *tmp = input;
    int rtn = OKAY; 
    input = fopen("gc4.ini", "r");
    if (input)
    {
	rtn = Interpret(NULL, result);
	fclose(input);
    }
    input = tmp;
    return rtn;
}

int main(int argc, char *argv[])
{
    String result;
    TraceThis();
    awk_init();
    ProcessArgs(argc, argv);
    InitVars();
    SetupInternalCommands();
    SetupPredefVars(argc, argv);
    int rtn = RunInitScript(result);
    if (rtn == OKAY)
	rtn = Interpret(NULL, result);
    if (rtn==ERROR)
    {
	if (errorInfo.Length()==0)
	{
	    errorInfo = "Exit or EOF";
	    rtn = OKAY;
	}
	errorInfo += (char)'\n';
        puts((char *)errorInfo);
    }
    if (rtn!=ERROR)
    {
	puts((char *)result);
	UserProcVariable *pv = (UserProcVariable *)LookupProc("atexit");
	if (pv)
	{
	    assert(!pv->IsInternal());
	    String ip(pv->Body());
	    (void)Interpret(&ip, result);
	}
    }
    if (profile) ShowProfiles();
    FreeVars();
    if (input != stdin) fclose(input);
    return exitcode;
}
