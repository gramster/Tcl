#include "tcl.h"

// parser returns

#define BARF	-2	// error
#define EFL	-1	// end-of-file
#define EOC	0	// end-of-command
#define IMM	1	// immediate arg
#define ARG	2	// normal arg

char *nms[] = { "EOC", "IMM", "ARG" };

int ch = '\n';
static int incomment;

extern int FindProc(char *nm);

static void Comment(InputString *ip);

void nextchar(InputString *ip)
{
    TraceThis();
    if (ip)
	ch = ip->Next();
    else if (feof(input))
        ch = ETX;
    else ch = fgetc(input);
}

static void skipspace(InputString *ip)
{
    TraceThis();
    while (ch==' ' || ch=='\t')
    	nextchar(ip);
}

static void Escape(InputString *ip, String &toke)
{
    int val;
    TraceThis();
    nextchar(ip);
    switch(ch)
    {
    case '\n':	toke += (char)' ';	break;
    case ' ':	toke += (char)' ';	break;
    case 'a':	toke += (char)7;	break;
    case 'b':	toke += (char)'\b';	break;
    case 'f':	toke += (char)'\f';	break;
    case 'n':	toke += (char)'\n';	break;
    case 'r':	toke += (char)'\r';	break;
    case 't':	toke += (char)'\t';	break;
    case 'v':	toke += (char)'\v';	break;
    case 'x':
	val = 0;
	for (;;)
	{
	    nextchar(ip);
	    if (isdigit(ch))
		ch -= '0';
	    else if (ch>='A' && ch<='F')
		ch -= 'A'-10;
	    else if (ch>='a' && ch<='a')
		ch -= 'a'-10;
	    else break;
	    val = val*16+ch;
	}
	toke += (int)val;;
	break;
    default:
	if (ch>='0' && ch<='7')
	{
	    val = 0;
	    while (ch>='0' && ch<='7')
	    {
		val = val * 8 + ch - '0';
	        nextchar(ip);
	    }
	    toke += (int)val;;
	}
	else toke += (char)ch;
	break;
    }
    nextchar(ip);
}

int ParseBlock(InputString *ip, String &toke)
{
    TraceThis();
    toke = "";
    int isBlock = 1;
    assert(ch=='{');
    nextchar(ip);
    while (ch != ETX)
    {
	switch(ch)
	{
	case '\\':
	    nextchar(ip);
	    if (ch=='\n')
		toke += (char)' ';
	    else
	    {
		toke += (char)'\\';
		toke += (char)ch;
	    }
	    nextchar(ip);
	    break;
	case '{':
	    isBlock++;
	    goto defalt;
	case '}':
	    assert(isBlock);
	    isBlock--;
	    if (isBlock == 0)
	    {
	        nextchar(ip);
	        return ARG;
	    }
	    // fall thru...
	default:
	defalt:
	    toke += (char)ch;
	    nextchar(ip);
	}
    }
    return BARF;
}

int ParseString(InputString *ip, String &toke, int end)
{
    TraceThis();
    String imm;
    toke = "";
    nextchar(ip);
    while (ch != ETX)
    {
	if (ch == end)
	{
	    nextchar(ip);
	    return ARG; // Done
	}
	else switch(ch)
	{
	case '\\':
	    Escape(ip, toke);
	    break;
	case '$':
	    if (ParseVariable(ip,toke)!=0)
		return BARF;
	    break;
        case '[':
	    imm = "";
	    if (ParseImmed(ip,imm)!=IMM)
		return BARF;
	    else
	    {
		if (Interpret(&imm, imm) != OKAY)
		{
		    toke = imm;
		    return BARF;
		}
		toke += imm;
	    }
	    break;
	defalt:
	default:
	    toke += (char)ch;
	    nextchar(ip);
	}
    }
    return BARF;
}

int ParseVariable(InputString *ip, String &toke, int expand)
{
    TraceThis();
    char *val;
    String v, *idx;
    assert(ch=='$');
    nextchar(ip);
    idx = NULL;
    if (ch=='{')
    {
    	nextchar(ip);
	while (ch!='}' && ch!=ETX)
	{
	    v += (char)ch;
    	    nextchar(ip);
	}
    	nextchar(ip);
    }
    else while (ch=='_' || isalnum(ch))
    {
    	v += (char)ch;
    	nextchar(ip);
    }
    if (ch == '(') // array?
    {
    	idx = new String;
	int rtn = ParseString(ip, *idx, ')');
    	if (rtn != IMM && rtn != ARG)
    	{
    	    toke = "array index error: ";
    	    toke += *idx;
	    delete idx;
	    return BARF;
	}
    }
    if (expand)
    {
        val = GetVar(v, idx?((char *)(*idx)):NULL);
        if (val == NULL)
        {
    	    toke = "undefined variable ";
    	    toke += v;
    	    if (idx)
    	    {
    	        toke += (char)'(';
    	        toke += *idx;
    	        toke += (char)')';
    	    }
	    if (idx) delete idx;
    	    return BARF;
        }
        else toke += val;
    }
    else
    {
	toke += (char)'$';
	if (idx)
	{
	    toke += v;
	    toke += (char)'(';
	    toke += *idx;
	    toke += (char)')';
	}
	else
	{
	    toke += (char)'{';
	    toke += v;
	    toke += (char)'}';
	}
    }
    if (idx) delete idx;
    return 0;
}

int ParseImmed(InputString *ip, String &toke)
{
    TraceThis();
    int isImmed = 1, isQuote = 0;
    assert(ch=='[');
    nextchar(ip);
    while (ch != ETX)
    {
	switch(ch)
	{
	case ' ':
	case '\t':
	    toke += (char)ch;
	    skipspace(ip); // save memory
	    break;
	case '"':
	    isQuote = 1 - isQuote;
	    goto defalt;
	case '[':
	    if (!isQuote) isImmed++;
	    goto defalt;
	case ']':
	    if (!isQuote)
	    {
	        assert(isImmed);
	        isImmed--;
	        if (isImmed == 0)
	        {
	            nextchar(ip);
	            return IMM;
	        }
	    }
	    goto defalt;
	case '\\':
	    Escape(ip, toke);
	    break;
//	case '$':
//	    if (ParseVariable(ip,toke,0)!=0)
//		return BARF;
//	    break;
	defalt:
	default:
	    toke += (char)ch;
	    nextchar(ip);
	}
    }
    return BARF;
}

int GetWord(InputString *ip, String &result);

// The discard flag is to allow the lookahead to be avoided
// at end-of-lines. Thus we can complete a command before
// reading the next line.

char discard = 0;

static int nextword(InputString *ip, String &token)
{
    TraceThis();
    token = "";
    if (discard && (ch == '\n' || ch==';'))
    {
	nextchar(ip);
	discard = 0;
    }
    for (;;)
    {
        skipspace(ip);
	if (ch != '#') break;
	while (ch!=ETX && ch!='\n') nextchar(ip);
    }
    if (ch == '\n' || ch==';')
    {
	discard = 1; // defer call to nextchar
	return EOC;
    }    
    if (ch == '"')
	return ParseString(ip, token, '"');
    else if (ch == '{')
	return ParseBlock(ip, token);
    else if (ch=='[')
	return ParseImmed(ip, token);
    while (ch != ETX)
    {
	switch(ch)
	{
	case ' ':
	case '\t':
	    nextchar(ip);
	    return ARG; // Done
	case ';':
	case '\n':
	    if (ch=='\n' && token.Length()==0)
	    {
	        nextchar(ip);
	        break;
	    }
	    else return ARG; // Done
	case '\\':
	    Escape(ip, token);
	    break;
	case '$':
	    if (ParseVariable(ip,token)!=0)
		return BARF;
	    break;
	defalt:
	default:
	    token += (char)ch;
	    nextchar(ip);
	}
    }
    if (token.Length()>0)
	return ARG;
    return EFL;
}

int GetWord(InputString *ip, String &result)
{
    TraceThis();
    int rtn = nextword(ip, result);
    if (debug)
    	if (rtn != EFL)
    	    fprintf(debug, "%s - <%s>\n", nms[rtn], (char *)result);
    	else
    	    fprintf(debug, "EFL - <%s>\n", (char *)result);
    if (rtn == IMM)
    {
	if (Interpret(&result, result) == ERROR)
	    rtn = BARF;
    }
    return rtn;
}

int ReadStatement(InputString *ip, StringGroup &args, String &result)
{
    TraceThis();
    String token;
    int rtn;
    int argcnt = 0;
    skipspace(ip);
    for (;;)
    {
	int rtn = GetWord(ip, token);
	switch(rtn)
	{
	case BARF:
	    result = token;
	    return -2;
	case EFL:
	    if (argcnt==0) return -1;
	    else return 0; //FindProc(args.GetString(0));
	case EOC:
	    if (argcnt > 0)
	        return 0; // FindProc(args.GetString(0));
	    break;
	case IMM:
	case ARG:
	    args.SetString(argcnt++, token);
	    break;
	}
    }
}

#if 0
int SplitList(String &list, StringGroup &elts, int lim)
{
    TraceThis();
    int oldch = ch;
    String token;
    InputString ip(list);
    nextchar(&ip);
    int cnt = 0;
    if (lim==0) lim=999;
    while (lim--)
    {
	int rtn = GetWord(&ip, token);
	switch(rtn)
	{
	case IMM:
	case ARG:
	    elts.SetString(cnt++, token);
	    break;
	default:
	    goto done;
	}
    }
done:
    ch = oldch;
    discard = 0;
    return cnt;
}

#else

int SplitList(char *p, StringGroup &elts, int lim)
{
    TraceThis();
    int cnt = 0;
    while (*p)
    {
	while (isspace(*p)) p++;
	if (*p==0) break;
	char *start = p;
	if (*p == '{')
	{
	    int nestcnt = 1;
	    start = ++p;
	    while (*p)
	    {
	        if (*p == '{') nestcnt++;
	        else if (*p == '}')
	        {
	    	    nestcnt--;
	    	    if (nestcnt==0)
	    	        break;
	        }
	        p++;
	    }
	}
	else while (*p && !isspace(*p)) p++;
	int oldch = *p;
	*p = 0;
	elts.SetString(cnt++, start);
	if (oldch)
	    *p++ = oldch;
	if (lim > 0)
	    if (--lim == 0) break;
    }
    return cnt;
}

int SplitList(String &list, StringGroup &elts, int lim)
{
    return SplitList((char *)list, elts, lim);
}

#endif

/****************************************************

	GC EXPRESSION EVALUATOR

 This code evaluates numeric expressions contained
 in a string. It should be portable across any
 decent C compiler.

 Entry to this code is through the routine:

	char *compute(char *str, int *rtn)
		
 which takes as arguments a pointer to the string
 containing the expression, and a pointer to the
 variable in which the result should be placed.
 If an error is found in the expression, a message
 is returned, but if the expression is acceptable
 then NULL is returned.

*****************************************************/

/*
 * Lexical tokens
 */

#define NUM_TOKAY		1
#define OR_TOKAY		2
#define AND_TOKAY		3
#define EQU_TOKAY		4
#define NEQ_TOKAY		5
#define ADD_TOKAY		6
#define SUB_TOKAY		7
#define MUL_TOKAY		8
#define DIV_TOKAY		9
#define MOD_TOKAY		10
#define LFT_TOKAY		11
#define RGT_TOKAY		12
#define GEQ_TOKAY		13
#define GTR_TOKAY		14
#define LEQ_TOKAY		15
#define LES_TOKAY		16
#define STR_TOKAY		17
#define NOT_TOKAY		18

/*
 * Local Variables
 */

static int	tok,	/* Current lexical token		*/
		tokval;	/* Current token value, if a number	*/

static char *expr = "";
static String fail;

/*
 * Forward declarations
 */

static int _cond_expr(InputString *ip, int &typ);

/*
 * Lexical analyser
 */

static String token;

// primary types : number, string or punctuation

#define NUMBER	0
#define STRING	1
#define PUNCT	2

static int Fail(char *msg)
{
    TraceThis();
    if (fail.Length()==0)
	fail = fail + msg + " in expression " + expr;
    return 1;
}

static void nexttok(InputString *ip)
{
    TraceThis();
    tok = -1;
    token = "";
    while (isspace(ch))
	nextchar(ip);
    if (ch != ETX)
    {
       	if (isdigit(ch))
	{
	    tok = NUM_TOKAY;
	    tokval = 0;
	    while (isdigit(ch))
	    {
	    	tokval = tokval * 10 + ch - '0';
	        nextchar(ip);
	    }
	}
	else
	{ /* Operator or string */
	    switch(ch)
	    {
	    case '|':
		tok = OR_TOKAY;
	        nextchar(ip);
	        break;
	    case '&':
	        tok = AND_TOKAY;
	        nextchar(ip);
	        break;
	    case '=':
	        nextchar(ip);
	        if (ch=='=')
	        {
	            nextchar(ip);
	    	    tok = EQU_TOKAY;
	        }
	        break;
	    case '!':
	        nextchar(ip);
	        if (ch=='=')
	        {
	            nextchar(ip);
	    	    tok = NEQ_TOKAY;
	        }
		else tok = NOT_TOKAY;
	        break;
	    case '+':
	        tok = ADD_TOKAY;
	        nextchar(ip);
	        break;
	    case '-':
	        tok = SUB_TOKAY;
	        nextchar(ip);
	        break;
	    case '*':
	        tok = MUL_TOKAY;
	        nextchar(ip);
	        break;
	    case '/':
	        tok = DIV_TOKAY;
	        nextchar(ip);
	        break;
	    case '%':
	        tok = MOD_TOKAY;
	        nextchar(ip);
	        break;
	    case '(':
	        tok = LFT_TOKAY;
	        nextchar(ip);
	        break;
	    case ')':
	        tok = RGT_TOKAY;
	        nextchar(ip);
	        break;
	    case '>':
	        nextchar(ip);
	        if (ch=='=')
	        {
	            nextchar(ip);
	            tok = GEQ_TOKAY;
	        }
	        else tok = GTR_TOKAY;
	        break;
	    case '[': // immed
	        if (ParseImmed(ip,token)==IMM)
		{
		    if (Interpret(&token, token) == ERROR)
			(void)Fail((char *)token);
		    else if (token.IsNumeric(&tokval))
			tok = NUM_TOKAY;
		    else
			tok = STR_TOKAY;
	        }
		else (void)Fail(token);
		break;
	    case '<':
	        nextchar(ip);
	        if (ch=='=')
	        {
	            nextchar(ip);
	            tok = LEQ_TOKAY;
	        }
	        else tok = LES_TOKAY;
	        break;
	    case '$': // variable
	        if (ParseVariable(ip,token)==0)
		{
		    if (token.IsNumeric(&tokval))
			tok = NUM_TOKAY;
		    else tok = STR_TOKAY;
		    break;
	        }
		else (void)Fail(token);
		//else (void)Fail("Bad variable");
		break;
	    case '{': // string
	        if (ParseBlock(ip,token)==0)
		{
		    if (token.IsNumeric(&tokval))
			tok = NUM_TOKAY;
		    else tok = STR_TOKAY;
		}
		else Fail("Bad block");
		break;
	    case '"': 
	        if (ParseString(ip,token,'"')==ARG)
		{
		    if (token.IsNumeric(&tokval))
			tok = NUM_TOKAY;
		    else tok = STR_TOKAY;
		}
		else (void)Fail("Bad string");
		break;
	    }
	}
    }
}

/*
 * PARSER
 */

/*
 * A primitive expression can be either a
 * conditional expression in parentheses,
 * or an integer.
 */

static int _prim_expr(InputString *ip, int &typ)
{
    TraceThis();
    int sgn = 1, v, not = 0;
    if (tok == NOT_TOKAY)
    {
	not = 1;
    	nexttok(ip);
    }
    if (tok==LFT_TOKAY)
    {
    	nexttok(ip);
    	v = _cond_expr(ip, typ);
    	if (tok!=RGT_TOKAY)
    	    return Fail(") expected");
    }
    else if (tok==SUB_TOKAY || tok==NUM_TOKAY)
    {
    	if (tok==SUB_TOKAY)
    	{
    	    sgn = -1;
    	    nexttok(ip);
    	}
    	if (tok!=NUM_TOKAY)
    	    return Fail("invalid integer");
    	v = sgn * tokval;
    	nexttok(ip);
	typ = NUMBER;
    }
    else if (tok==STR_TOKAY)
    {
    	nexttok(ip);
	typ = STRING;
    }
    else return Fail("String or integer expected");
    return not ? !v : v;
}

/*
 * Multiplicative expression
 */

static int _mul_expr(InputString *ip, int &typ)
{
    TraceThis();
    int v = _prim_expr(ip, typ);
    if (tok == MUL_TOKAY || tok == DIV_TOKAY || tok == MOD_TOKAY)
    {
	if (typ != NUMBER)
	    return Fail("Integer expected");
	int op = tok;
   	nexttok(ip);
    	int rv = _mul_expr(ip, typ);
	if (typ != NUMBER)
	    return Fail("Integer expected");
    	if (op==MUL_TOKAY)
	    v *= rv;
    	else if (tok==DIV_TOKAY)
	    v /= rv;
    	else
	    v %= rv;
    }
    return v;
}

/*
 * Additive expression
 */

static int _add_expr(InputString *ip, int &typ)
{
    TraceThis();
    int v = _mul_expr(ip, typ);
    if (tok == ADD_TOKAY || tok == SUB_TOKAY)
    {
        int op = tok; 
        if (typ != NUMBER)
	    return Fail("Integer expected");
        nexttok(ip);
        int rv = _add_expr(ip, typ);
        if (typ != NUMBER)
	    return Fail("Integer expected");
        if (op==ADD_TOKAY)
    	    v += rv;
        else
    	    v -= rv;
    }
    return v;
}

/*
 * Relational expression
 */

static int _rel_expr(InputString *ip, int &typ)
{
    TraceThis();
    String l(token);
    int v = _add_expr(ip, typ);
    if (tok==GTR_TOKAY || tok == GEQ_TOKAY || tok==LEQ_TOKAY || tok == LES_TOKAY)
    {
	int op = tok;
    	nexttok(ip);
	String r(token);
	if (typ == STRING)
	{
	    if (tok == STR_TOKAY)
	    {
    	        v = Compare(l,r);
    	        nexttok(ip);
		typ = NUMBER;
	    }
	    else return Fail("String expected");
	}
	else
	{
    	    v -= _rel_expr(ip, typ);
	    if (typ != NUMBER)
	    	return Fail("Integer expected");
	}
        if (op==GTR_TOKAY)
	    v = (v > 0);
        else if (op==GEQ_TOKAY)
	    v = (v >= 0);
        else if (op==LEQ_TOKAY)
	    v = (v <= 0);
	else
	    v = (v < 0);
    }
    return v;
}

/*
 * Equality expression
 */

static int _equ_expr(InputString *ip, int &typ)
{
    TraceThis();
    String l(token);
    int v = _rel_expr(ip, typ);
    if (tok==EQU_TOKAY || tok == NEQ_TOKAY)
    {
	int invert = (tok == NEQ_TOKAY);
    	nexttok(ip);
	String r(token);
	if (typ == STRING)
	{
	    if (tok == STR_TOKAY)
	    {
    	        v = (Compare(l,r)==0);
    	        nexttok(ip);
		typ = NUMBER;
	    }
	    else return Fail("String expected");
	}
	else
	{
    	    v = (v == _equ_expr(ip, typ));
	    if (typ != NUMBER)
	    	return Fail("Integer expected");
	}
	if (invert)
	    v = !v;
    }
    return v;
}

/*
 * AND conditional expressions
 */

static int _and_expr(InputString *ip, int &typ)
{
    TraceThis();
    int v = _equ_expr(ip, typ);
    if (tok==AND_TOKAY)
    {
	if (typ != NUMBER)
	    return Fail("Integer expected");
   	nexttok(ip);
    	v &= _and_expr(ip, typ);
	if (typ != NUMBER)
	    return Fail("Integer expected");
    }
    return v;
}

/*
 * OR conditional expressions
 */

static int _cond_expr(InputString *ip, int &typ)
{
    TraceThis();
    int v = _and_expr(ip, typ);
    if (tok==OR_TOKAY)
    {
	if (typ != NUMBER)
	    return Fail("Integer expected");
   	nexttok(ip);
    	v |= _cond_expr(ip, typ);
	if (typ != NUMBER)
	    return Fail("Integer expected");
    }
    return v;
}

/*
 * Main entry point. The computed value is stored in the
 * area pointed to by *rtn. `compute' returns NULL upon
 * success, or an error message otherwise.
 */

char *compute(char *str, int *rtn)
{
    TraceThis();
    int typ;
    int oldch = ch;
    ch = ' ';
    expr = str;
    fail = "";
    InputString ip(str);
    nexttok(&ip);
    *rtn = _cond_expr(&ip, typ);
    ch = oldch;
    if (typ != NUMBER)
	Fail("Integer type expected");
    else if (tok!=-1)
	Fail("Syntax error");
    if (fail.Length())
	return (char *)fail;
    return NULL;
}



