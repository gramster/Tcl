/*
 * awklib.h - defines, global variables, and function prototypes for
 * AWKLIB routines.
 *
 * Copyright 1988, Jim Mischel.  All rights reserved.
 *
 */
extern int RSTART;
extern int RLENGTH;
extern int NF;
extern char *FS;
extern char *FS_PAT;
extern char *FIELDS[];

#define MAXSTR	1024
#define MAXPAT	2*MAXSTR

#ifdef __cplusplus
extern "C" {
#endif
void	awk_init (void);
char *	setfs 	(char *fs);
char * 	match 	(char *s, char *re);
char * 	re_match (char *s, char *pat);
char * 	makepat 	(char *re, char *pat);
int 	split 	(char *s, char **a, char *fs);
int	re_split (char *s, char **a, char *pat);
int	getline 	(char *s, int nchar, FILE *infile);
int	sub 	(char *re, char *replace, char *str);
int	re_sub 	(char *pat, char *replace, char *str);
int	gsub 	(char *re, char *replace, char *str);
int	re_gsub 	(char *pat, char *replace, char *str);
char * 	strins 	(char *s, char *i, int pos);
char * 	strcins 	(char *s, int ch, int pos);
char * 	strdel 	(char *s, int pos, int n);
char * 	strcdel 	(char *s, int pos);
char * 	strccat 	(char *s, int c);
#ifdef __cplusplus
}
#endif



