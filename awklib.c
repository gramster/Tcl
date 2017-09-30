/*
 * awklib.c - C callable routines that provide field splitting and regular
 *  expression matching functions much like those found in AWK.
 *
 * Copyright 1988, Jim Mischel.  All rights reserved.
 */

#pragma hdrstop
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define	TRUE		-1
#define	FALSE		0
#define ALMOST		1	/* returned when a closure matches a NULL */

#define ENDSTR		'\0'
#define EOL		'$'
#define BOL		'^'
#define NEGATE		'^'
#define CCL		'['
#define NCCL		']'
#define CCLEND		']'
#define ANY		'.'
#define DASH		'-'
#define OR		'|'
#define ESCAPE		'\\'
#define LPAREN		'('
#define RPAREN		')'
#define CLOSURE		'*'
#define POS_CLO 	'+'
#define ZERO_ONE 	'?'
#define LITCHAR 	'c'
#define END_TERM 	'e'
#define FS_DEFAULT 	"[ \t]+"

#define MAXSTR		1024
#define MAXPAT		2*MAXSTR
#define MAXFIELD 	128

/*
 * AWKLIB global variables.  These variables are defined in AWKLIB.H and may
 * be accessed by the application.
 */
int 	RSTART;			/* start of matched substring */
int	RLENGTH;		/* length of matched substring */
int    	NF; 			/* number of fields from most current split */
char *	FS;			/* global field separator */
char *	FS_PAT;                 /* compiled field separator */
char *  FIELDS[MAXFIELD];       /* contents of fields from most current split */

/*
 * Internal function prototypes.
 */
char * 	re_match (char *s, char *pat);
char * 	makepat 	(char *re, char *pat);
int	re_split (char *s, char **a, char *pat);
char * 	do_sub (char *str, int inx, int len, char *replace);
char 	parse_escape (void);

/*
 * These string routines, while designed specifically for this application,
 * may be useful to other programs.  Their prototypes are included in the
 * AWKLIB.H file.
 */
char * 	strins 	(char *s, char *i, int pos);
char * 	strcins 	(char *s, int ch, int pos);
char * 	strdel 	(char *s, int pos, int n);
char * 	strcdel 	(char *s, int pos);
char * 	strccat 	(char *s, int c);

/*
 * Initialize AWKLIB global variables.  This routine MUST be called before
 * using the AWKLIB routines.  Failure to do so may produce some strange
 * results.
 */
void	awk_init (void) {
    int x;
    char * setfs (char *fs);

    FS = FS_PAT = NULL;
    setfs (FS_DEFAULT);
    RSTART = RLENGTH = NF = 0;
    for (x = 0; x < MAXFIELD; x++)
	FIELDS[x] = NULL;
} /* awk_init */

/*
 * Sets the field separator to the regular expression fs.  The regular
 * expression is compiled into FS_PAT.  FS_PAT is returned.  NULL is returned
 * on error and neither FS or FS_PAT is modified.
 */
char *	setfs (char *fs) {
    char pat[MAXPAT];

    pat[0] = ENDSTR;
    if (makepat (FS_DEFAULT, pat) == NULL)
	return (NULL);
    if (FS != NULL)
        free (FS);
    if (FS_PAT != NULL)
	free (FS_PAT);
    FS = strdup (fs);
    FS_PAT = strdup (pat);
    return (FS_PAT);
} /* setfs */

/*
 * makepat() - "compile" the regular expression re into pat and return a
 * a pointer to the compiled string, or NULL if the compile fails.
 *
 * Performs a recursive descent parse of the expression.
 */

char *_re_ptr;		/* global for pattern building */

char * makepat (char *re, char *pat) {
    char *t;
    char * parse_expression (void);

    _re_ptr = re;
    if ((t = parse_expression ()) == NULL)
	return (NULL);
    else if (*_re_ptr != ENDSTR) {
	free (t);
	return (NULL);
    }
    else {
	strcpy (pat, t);
	free (t);
	return (pat);
    }
} /* makepat */

/*
 * parse_expression() - Parse and translate an expression.  Returns a pointer
 * to the compiled expression, or NULL on error.
 */
char * parse_expression (void) {
    char pat[MAXPAT];
    char *arg1;

    char * parse_term (void);

    pat[0] = ENDSTR;
    if ((arg1 = parse_term ()) == NULL)	/* get the first term */
	return (NULL);

    while (*_re_ptr == OR) {		/* parse all subsequent terms */
        strccat (pat, OR);
	strcat (pat, arg1);
        strccat (pat, END_TERM);
	free (arg1);
	_re_ptr++;
        if ((arg1 = parse_term ()) == NULL)
	    return (NULL);
    }
    strcat (pat, arg1);
    strccat (pat, END_TERM);
    free (arg1);
    return (strdup (pat));
} /* parse_expression */

/*
 * parse_term() - parse and translate a term.  Returns a pointer to the
 * compiled term or NULL on error.
 */
char * parse_term (void) {
    char *t;
    char pat[MAXPAT];

    int isfactor (char c);
    char * parse_factor (void);

    pat[0] = ENDSTR;
    if (*_re_ptr == BOL)
        strccat (pat, *_re_ptr++);
    do {
        if ((t = parse_factor ()) == NULL)
	    return (NULL);
	else {
	    strcat (pat, t);
	    free (t);
	}
    } while (isfactor (*_re_ptr));	/* parse all factors of this term */
    return (strdup (pat));
} /* parse_term */

/*
 * isfactor() - returns TRUE if c is a valid factor character
 */
int 	isfactor (char c) {
    static char nfac_chars[] = "^|)]+?*";
    return (strchr (nfac_chars, c) == NULL) ? TRUE : FALSE;
} /* isfactor */

/*
 * parse_factor() - parse and translate a factor.  Returns a pointer to the
 * compiled factor or NULL on error.
 */
char * parse_factor (void) {
    char pat[MAXPAT];
    char *t;

    char * parse_expression (void);
    int parse_closure (char *pat, char c);
    char * parse_ccl (void);

    pat[0] = ENDSTR;
    switch (*_re_ptr) {
	case LPAREN	:		/* parenthesised expression */
	    _re_ptr++;
	    t = parse_expression ();
	    strcat (pat, t);
	    free (t);
	    if (*_re_ptr++ != RPAREN)
		return (NULL);
	    break;
        case CCL	:		/* character class */
	    _re_ptr++;
	    t = parse_ccl ();
	    strcat (pat, t);
	    free (t);
	    if (*_re_ptr++ != CCLEND)
		return (NULL);
	    break;
	case ANY	:		/* '.' or '$' operators */
	case EOL	:
	    strccat (pat, *_re_ptr++);
	    break;
        case ESCAPE	:		/* ESCAPE character */
	    _re_ptr++;
            strccat (pat, LITCHAR);
            strccat (pat, parse_escape ());
	    break;
	case CLOSURE 	:
	case POS_CLO 	:
	case ZERO_ONE 	:
        case NEGATE	:
	case CCLEND	:
	case RPAREN	:
	case OR		:	 	/* not valid characters */
 	    return (NULL);
	default	  	:		/* literal character */
            strccat (pat, LITCHAR);
	    strccat (pat, *_re_ptr++);
	    break;
    }
    /*
     * check for closure
     */
    if (*_re_ptr == CLOSURE || *_re_ptr == ZERO_ONE || *_re_ptr == POS_CLO)
	if (parse_closure (pat, *_re_ptr++) == FALSE)
	    return (NULL);
    return (strdup (pat));
} /* parse_factor */

/*
 * parse_escape () - returns ASCII value of character(s) following ESCAPE
 */
char 	parse_escape (void) {
    unsigned char ch;
    switch (*_re_ptr) {
	case 'b'  : _re_ptr++; return ('\b');	/* backspace */
	case 't'  : _re_ptr++; return ('\t');	/* tab */
	case 'f'  : _re_ptr++; return ('\f');	/* formfeed */
	case 'n'  : _re_ptr++; return ('\n');	/* linefeed */
	case 'r'  : _re_ptr++; return ('\r');	/* carriage return */
        case '0'  :				/* 0-7 is octal constant */
	case '1'  :
	case '2'  :
	case '3'  :
	case '4'  :
	case '5'  :
	case '6'  :
	case '7'  :
            ch = *_re_ptr++ - '0';
	    if (*_re_ptr >= '0' && *_re_ptr < '8') {
	        ch <<= 3;
		ch += *_re_ptr++ - '0';
	    }
	    if (*_re_ptr >= '0' && *_re_ptr < '8') {
	        ch <<= 3;
	        ch += *_re_ptr++ - '0';
	    }
	    return (ch);
        default	  :			/* otherwise, just that char */
	    return (*_re_ptr++);
    }
} /* parse_escape */

/*
 * parse_closure() - place closure character and size before the factor
 * in the compiled string.
 */
int parse_closure (char *pat, char c) {
    int len;

    memmove (pat+2, pat, strlen (pat) + 1);
    pat[0] = c;
    len = strlen (pat + 2);
    if (len > 255)
	return (FALSE);			/* closure expression too large */
    else {
        pat[1] = len;
	return (TRUE);
    }
} /* parse_closure */

/*
 * parse_ccl() - parse and translate a character class.  Return pointer to the
 * compiled class or NULL on error.
 */
char * parse_ccl (void) {
    char pat[MAXPAT];
    int first = TRUE;
    int len;

    char * parse_dash (char *pat, char ch);

    strcpy (pat, "[ ");
    if (*_re_ptr == NEGATE) {		/* if first character is NEGATE */
	pat[0] = NCCL;			/* then we have a negated */
	_re_ptr++;			/* character class */
    }

    /*
     * parse all characters up to the closing bracket or end of string marker
     */
    while (*_re_ptr != CCLEND && *_re_ptr != ENDSTR) {
        if (*_re_ptr == DASH && first == FALSE) {	/* DASH, check for range */
            if (*++_re_ptr == NCCL)
		strccat (pat, DASH);		/* not range, literal DASH */
	    else
	        parse_dash (pat, *_re_ptr++);
	}
	else {
	    if (*_re_ptr == ESCAPE) {
	        _re_ptr++;
                strccat (pat, parse_escape ());
	    }
	    else
	        strccat (pat, *_re_ptr++);
	}
	first = FALSE;
    }
    len = strlen (pat+2);
    if (len > 255)
	return (NULL);			/* character class too large */
    else {
        pat[1] = len;			/* store CCL length at pat[1] */
	return (strdup (pat));
    }
} /* parse_ccl */

/*
 * parse_dash() - fill in range characters.
 */
char * parse_dash (char *pat, char ch) {
    int ch1;

    for (ch1 = pat[strlen (pat) - 1] + 1; ch1 <= ch; ch1++)
        strccat (pat, ch1);
    return (pat);
} /* parse_dash */

/*
 * match() - Return a pointer to the first character of the left-most longest
 * substring of s that matches re or NULL if no match is found.  Sets
 * RSTART and RLENGTH.  This routine compiles the regular expression re and
 * then calls re_match to perform the actual matching.
 */
char * match (char *s, char *re) {
    char pat[MAXPAT];

    pat[0] = ENDSTR;
    if (makepat (re, pat) == NULL)
	return (NULL);
    return (re_match (s, pat));
} /* match */

/*
 * re_match() - Return a pointer to the first character of the left-most
 * longest substring of s that matches pat, or NULL if no match is found.
 * Sets RSTART and RLENGTH.  The != FALSE test below must NOT be changed
 * to == TRUE.  match_term() can return TRUE, FALSE, or ALMOST.  Both TRUE
 * and ALMOST are considered TRUE by this routine.
 */
char *_s_end;			/* global points to last character matched */

char * re_match (char *s, char *pat) {
    char *c = s;
    int match_term (int inx, char *s, char *pat);

    _s_end = NULL;
    while (*c != ENDSTR) {
	if (match_term (c-s, c, pat) != FALSE) {
	    RSTART = c-s;
	    RLENGTH = _s_end - c;
	    return (c);
	}
	c++;
    }
    RSTART = RLENGTH = 0;
    return (NULL);
} /* re_match */

/*
 * Match a compiled term.  Returns TRUE, FALSE, or ALMOST.
 */
int match_term (int inx, char *s, char *pat) {
    int match_or (int inx, char *s, char *pat);
    int match_ccl (char c, char *pat);
    int match_closure (int inx, char *s, char *pat, char *clopat);
    int match_0_1 (int inx, char *s, char *pat);

    _s_end = s;
    if (*pat == ENDSTR)
	return (FALSE);
    do {
	switch (*pat) {
	    case BOL	  :			/* match beginning of line */
		if (inx != 0)
		    return (FALSE);
		pat++;
		break;
	    case LITCHAR  :			/* match literal character */
		if (*s++ != *++pat)
		    return (FALSE);
		pat++;
		break;
	    case END_TERM : pat++;  break;	/* skip end-of-term character */
	    case ANY	  :			/* match any character ... */
		if (*s++ == ENDSTR)		/* ... except end of string */
		    return (FALSE);
		pat++;
		break;
	    case OR	  : return (match_or (inx, s, pat));
	    case CCL 	  :			/* character class requires */
	    case NCCL	  :                     /* special processing */
		if (*s == ENDSTR)
		    return (FALSE);
		if (!match_ccl (*s++, pat++))
		    return (FALSE);
		pat += *pat + 1;
		break;
	    case EOL	  :			/* match end of string */
		if (*s != ENDSTR)
		    return (FALSE);
		pat++;
		break;
	    case ZERO_ONE : return (match_0_1 (inx, s, pat));
	    case CLOSURE  :
	    case POS_CLO  : {
		char clopat[MAXPAT];
		strncpy (clopat, pat+2, *(pat+1));
		clopat[*(pat+1)] = ENDSTR;
		return (match_closure (inx, s, pat, clopat));
		break;
	    }
	    default	  :
	    /*
	     * If we get to this point, then something has gone very wrong.
	     * Most likely, someone has tried to match with an invalid
	     * compiled pattern.  Whatever the case, the only thing to do
	     * is abort the program.
	     */
		fputs ("In match_term:  can't happen", stderr);
		exit (1);
		break;
	} /* switch */
	_s_end = s;
    } while (*pat != ENDSTR);
    return (TRUE);
} /* match_term */

/*
 * match_or() - Handles selection processing.
 */
int match_or (int inx, char *s, char *pat) {
    char workpat[MAXPAT];
    char *t1, *t2, *junk;

    int match_term (int inx, char *s, char *pat);
    char * skip_term (char *pat);

    /*
     * The first case is build into workpat.  Second case is already there.
     * Both patterns are searched to determine the longest matched substring.
     */
    workpat[0] = ENDSTR;
    pat++;
    junk = skip_term (pat);
    strncat (workpat, pat, junk-pat);
    strcat (workpat, skip_term (junk));
    t1 = (match_term (inx, s, workpat) != FALSE) ? _s_end : NULL;
    /*
     * The second pattern need not be searched if the first pattern results
     * in a match through to the end of the string, since the longest possible
     * match has already been found.
     */
    if (t1 == NULL || *_s_end != ENDSTR) {
	t2 = (match_term (inx, s, junk) != FALSE) ? _s_end : NULL;
	/*
	 * determine which matched the longest substring
	 */
	if (t1 != NULL && (t2 == NULL || t1 > t2))
	    _s_end = t1;
    }
    return (t1 == NULL && t2 == NULL) ? FALSE : TRUE;
} /* match_or */

/*
 * Skip over the current term and return a pointer to the next term in
 * the pattern.
 */
char * skip_term (char *pat) {
    register int nterm = 1;

    while (nterm > 0) {
        switch (*pat) {
	    case OR	: nterm++; break;
	    case CCL	:
	    case NCCL	:
	    case CLOSURE:
	    case ZERO_ONE:
	    case POS_CLO:
		pat++;
                pat += *pat;
		break;
	    case END_TERM: nterm--; break;
	    case LITCHAR: pat++; break;
	}
	pat++;
    }
    return (pat);
} /* skip_term */

/*
 * Match the ZERO_ONE operator.  First, this routine attempts to match the
 * entire pattern with the input string.  If that fails, it skips over
 * the closure pattern and attempts to match the rest of the pattern.
 */
int match_0_1 (int inx, char *s, char *pat) {
    char *save_s = s;

    if (match_term (inx, s, pat+2) == TRUE)
	return (TRUE);
    else if (match_term (inx, save_s, pat+2+*(pat+1)) == FALSE)
	return (FALSE);
    else
	return (ALMOST);
} /* match_0_1 */

/*
 * Match CLOSURE and POS_CLO.
 * Match as many of the closure patterns as possible, then attempt to match
 * the remaining pattern with what's left of the input string.  Backtrack
 * until we've either matched the remaing pattern or we arrive back at where
 * we started.
 */
int match_closure (int inx, char *s, char *pat, char *clopat) {
    char *save_s = s;

    if (match_term (inx, s, clopat) == TRUE) {
	save_s = _s_end;
	if (match_closure (inx, save_s, pat, clopat) == TRUE)
	    return (TRUE);
	else
	    return (match_term (inx, save_s, pat+2+*(pat+1)));
    }
    else if (*pat != CLOSURE)
	return (FALSE);		  /* POS_CLO requires at least one match */
    else if (match_term (inx, save_s, pat+2+*(pat+1)) == TRUE)
	return (ALMOST);
    else
	return (FALSE);
} /* match_closure */

/*
 * Match a character class or negated character class
 */
int match_ccl (char c, char *pat) {
    register int x;
    char ccl = *pat++;

    for (x = *pat; x > 0; x--)
        if (c == pat[x])
	    return (ccl == CCL);
    return (ccl != CCL);
} /* match_ccl */

/*
 * Substitue 'replace' for the leftmost longest substring of str matched by
 * the regular expression re.
 * Return number of substitutions made (which in this case will be 0 or 1).
 */
int sub (char *re, char *replace, char *str) {

    if (match (str, re) != NULL) {
	free (do_sub (str, RSTART, RLENGTH, replace));
	return (1);
    }
    else
	return (0);
} /* sub */

/*
 * Substitue 'replace' for the leftmost longest substring of str matched by
 * the compiled regular expression pat.
 * Return number of substitutions made (which in this case will be 0 or 1).
 */
int re_sub (char *pat, char *replace, char *str) {
    int	re_sub 	(char *pat, char *replace, char *str);

    if (re_match (str, pat) != NULL) {
	free (do_sub (str, RSTART, RLENGTH, replace));
	return (1);
    }
    else
	return (0);
} /* re_sub */

/*
 * Substitute 'replace' globally for all substrings in str matched by the
 * regular expression re.
 * Return number of substitutions made.
 *
 * This routine uses makepat() to compile the regular expression, then calls
 * re_gsub() to do the actual replacement.
 *
 * NOTE:  gsub() makes only 1 pass through the string.  Replaced strings
 * cannot themselves be replaced.
 */
int gsub (char *re, char *replace, char *str) {
    int	re_gsub (char *pat, char *replace, char *str);

    char pat[MAXPAT];

    pat[0] = ENDSTR;
    if (makepat (re, pat) == NULL)
	return (0);
    return (re_gsub (pat, replace, str));
} /* gsub */

/*
 * Substitute 'replace' globally for all substrings in str matched by the
 * compiled regular expression pat.
 * Return number of substitutions made.
 *
 * NOTE:  gsub() makes only 1 pass through the string.  Replaced strings
 * cannot themselves be replaced.
 */
int re_gsub (char *pat, char *replace, char *str) {
    char *m = str;
    int nsub = 0;
    char *p;

    while ((m = re_match (m, pat)) != NULL) {
	p = do_sub (m, 0, RLENGTH, replace);
	nsub++;
	m += strlen (p);
	free (p);
    }
    return (nsub);
} /* re_gsub */

/*
 * remove 'len' characters from 'str' starting at position 'inx'.  Then insert
 * the replacement string at position 'inx'.
 */
char * do_sub (char *str, int inx, int len, char *replace) {
    char *p;
    char * makesub (char *replace, char *found, int len);

    p = makesub (replace, &str[inx], len);
    strdel (str, inx, len);
    strins (str, p, inx);
    return (p);
} /* do_sub */

/*
 * Make a substitution string.
 */
char * makesub (char *replace, char *found, int len) {
    char news[MAXSTR];
    char *c = replace;
    int x;

    news[0] = ENDSTR;
    while (*c != ENDSTR) {
	if (*c == '&')
	    for (x = 0; x < len; x++)
		strccat (news, found[x]);
	else if (*c == '\\') {
		_re_ptr = c+1;
		strccat (news, parse_escape ());
		c = _re_ptr - 1;
	}
	else
	    strccat (news, *c);
	c++;
    }
    return (strdup (news));
} /* makesub */

/*
 * split - split the string s into fields in the array a on field separator fs.
 * fs is a regular expression.  Returns number of fields.  Also sets the global
 * variable NF.  This routine compiles fs into a pattern and then calls
 * re_split() to do the work.
 */
int split (char *s, char **a, char *fs) {
    char pat[MAXPAT];

    pat[0] = ENDSTR;
    makepat (fs, pat);
    return re_split (s, a, pat);
} /* split */

/*
 * re_split() - split the string s into fields in the array on field seperator
 * pat.  pat is a compiled regular expression (built by makepat()).  Returns
 * number of fields.  Also sets the global variable NF.
 */
int re_split (char *s, char **a, char *pat) {
    int rstart = RSTART;		/* save RSTART and RLENGTH */
    int rlength = RLENGTH;
    char *c = s;
    char *oldc = s;

    NF = 0;
    if (a[0] != NULL)
	free (a[0]);
    a[0] = strdup (s);

    while (*oldc != ENDSTR) {
	while ((c = re_match (oldc, pat)) == oldc)
	    oldc += RLENGTH;
	if (*oldc != ENDSTR) {
	    if (c == NULL)
		c = &oldc[strlen (oldc)];
	    if (a[++NF] != NULL)
		a[NF] = (char *)realloc (a[NF], c-oldc+1);
            else
                a[NF] = (char *)malloc (c-oldc+1);
            memcpy (a[NF], oldc, c-oldc);
	    a[NF][c-oldc] = ENDSTR;
	    oldc = c;
	}
    }
    RSTART = rstart;			/* restore globals */
    RLENGTH = rlength;
    return (NF);
} /* re_split */

/*
 * Reads a line from infile and splits it into FIELDS.  Returns EOF on
 * end-of-file or error.
 */
int getline (char *s, int nchar, FILE *infile) {
    char *c;

    if (fgets (s, nchar, infile) == NULL)
	return (EOF);
    if ((c = strchr (s, '\n')) != NULL)
	*c = ENDSTR;                    	/* look for and replace newline */
    re_split (s, FIELDS, FS_PAT);
    return (0);
} /* getline */

/*
 * add a character to the end of a string
 */
char * strccat (char *s, int ch) {
    register int len = strlen (s);
    s[len++] = ch;
    s[len] = ENDSTR;
    return (s);
}

/*
 * removes the character at pos from the string.
 */
char * strcdel (char *s, int pos) {
    memcpy (s+pos, s+pos+1, strlen (s) - pos);
    return (s);
} /* strcdel */

/*
 * inserts the character ch into the string at position pos.  Assumes there
 * is room enough in the string for the character.
 */
char * strcins (char *s, int ch, int pos) {
    memmove (s+pos+1, s+pos, strlen (s) - pos + 1);
    s[pos] = ch;
    return (s);
}

/*
 * removes n characters from s starting at pos
 */
char * strdel (char *s, int pos, int n) {
    memcpy (s+pos, s+pos+n, strlen(s)-pos-n+1);
    return (s);
}

/*
 * inserts the string i into the string s at position pos.  Assumes there
 * is sufficient memory in s to hold i.
 */
char * strins (char *s, char *i, int pos) {
    char *p = s+pos;
    int ilen = strlen (i);

    memmove (p+ilen, p, strlen (s) - pos + 1);
    memcpy (p, i, ilen);
    return (s);
}



