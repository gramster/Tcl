/*
 * A drop-in debugging library for C v1.0
 *
 * (c) 1994 by Graham Wheeler, All Rights Reserved
 *
 * This library provides drop-in wrappers for the following
 * common C library routines:
 *
 * Memory allocation routines:
 *	malloc(), calloc(), free(), realloc()
 *
 * String and memory routines:
 *	strcpy(), strncpy(), memcpy(), memset(), memmove(),
 *	strcmp(), strncmp(), memcmp(), strlen(), strdup(),
 *	strstr(), strpbrk(), stpcpy(), strcat(), strncat(),
 *	strchr(), strrchr(), strspn(), strcspn()
 *
 * File I/O routines:
 *	open(), close(), dup(), fopen(), fclose(),
 *	read(), fread(), fgets()
 * 
 * The library catches:
 *
 * - failed mallocs/callocs/reallocs and bad frees
 * - memory and file leaks
 * - some bad parameters passed to these routines
 *
 * In addition:
 *
 * - some overruns of dynamic, global and auto buffers are caught,
 *	reported and recovered from (memcpy, memset,
 *	strcpy, strncpy, stpcpy, read, fread, fgets)
 * - all overruns of dynamic buffers are reported when the memory
 *	is freed. Overruns of less than 5 bytes will not clobber the
 *	heap.
 *
 * To use the library, link this file with your application.
 * Add a line `#include "gwdebug.h" to each source file of
 * your application, AFTER any other #includes.
 * Then compile with GW_DEBUG defined to use the library.
 *
 * You can define DEBUG_LOG to be a log file name; else
 * standard error is used.
 *
 * TODO:
 * Test all functions not yet done in mytest.c.
 */

#ifdef GW_DEBUG

#include <stdlib.h>
#include <stdio.h>
#ifdef __MSDOS__
#include <io.h>
#endif
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#ifdef LOCAL_HEAP
#include "heap.h"
#endif

static FILE *logfile = NULL;
static char *store_name(char *name);
static void my_initialise(void);
static void my_report(void);

/*******************************/
/* Memory allocation debugging */
/*******************************/

#define MAGIC			(0x24681357UL)
#define GETF(p,o)		((long *)(p)) [-o] 
#define NBYTES(p)		((int)(GETF(p,1)))
#define NEXT(p)		((void *)(GETF(p,2)))
#define FILE(p)		((char *)(GETF(p,3)))
#define LINE(p)		((int)(GETF(p,4)))
#define MAGIC1(p)		((unsigned long)(GETF(p,5)))
#define MAGIC2(p)		((unsigned long)(GETF(((char *)p)+NBYTES(p),0)))

#define SET_NBYTES(p,n)	GETF(p,1) = (long)(n)
#define SET_NEXT(p,n)		GETF(p,2) = (long)(n)
#define SET_FILE(p,f)		GETF(p,3) = (long)(f)
#define SET_LINE(p,l)		GETF(p,4) = (long)(l)
#define SET_MAGIC1(p,m)	GETF(p,5) = (long)(m)
#define SET_MAGIC2(p,m)	GETF(((char *)p)+NBYTES(p),0) = (long)(m)

void my_free(void *p, char *f, int l);

static void *heap_list_head;
static int done_shutdown = 0;

static int alloc_cnt = 0, free_cnt = 0;
static long mem_alloc = 0l, max_alloc = 0l;

void my_memory_initialise(void)
{
	heap_list_head = NULL;
}

void my_memory_report(int is_last)
{
	void *p;
	/* walk dat list... */
	p = heap_list_head;
	if (p)
	{
		fprintf(logfile,is_last ? "MEMORY LEAKS:\n" : "Allocated Memory Blocks:\n");
		while (p)
		{
			void *old_p = p;
			fprintf(logfile,"\tSize %8d File %16s Line %d\n", NBYTES(p), FILE(p), LINE(p));
			p = NEXT(p);
			if (is_last)
				my_free(old_p, __FILE__, __LINE__);
		}
		fprintf(logfile, "%d allocs, %d frees\n", alloc_cnt, free_cnt);
		fprintf(logfile, "Currently allocated: %ld (Max %ld)\n", mem_alloc, max_alloc);
	}
	if (is_last) done_shutdown = 1;
}

void *my_calloc(size_t n, char *f, int l)
{
	char *savedname;
	/* Allocate the memory with space enough for our info */
	void *rtn = calloc(n + 6 * sizeof(long), 1);
	if (!logfile) my_initialise();
	assert(rtn);
	/* Get the pointer that is returned to the user */
	rtn = (void *)(((char *)rtn) + 5 * sizeof(long));
	/* Save the size information */
	SET_NBYTES(rtn,n);
	/* Prepend to front of heap list */
	SET_NEXT(rtn,heap_list_head); heap_list_head = rtn;
	/* Save (or re-use previously saved) file name */
	/* Save the file name and line number info, and put in the bounding
		magic number markers */
	SET_FILE(rtn, store_name(f));
	SET_LINE(rtn,l);
	SET_MAGIC1(rtn,MAGIC);
	SET_MAGIC2(rtn,MAGIC);
	alloc_cnt++;
	mem_alloc += n;
	if (mem_alloc > max_alloc)
		max_alloc = mem_alloc;
	return rtn;
}

void my_free(void *p, char *f, int l)
{
	void *tmp, *last = NULL;
	if (!logfile) my_initialise();
	/* Search the list for the block */
	tmp = heap_list_head;
	while (tmp)
	{
#ifdef __MSDOS__
		if ( ((void far *)tmp) == ((void far *)p) ) break;
#else
		if ( ((void *)tmp) == ((void *)p) ) break;
#endif
		else if (MAGIC1(tmp) != MAGIC) goto error;
		else
		{
			last = tmp;
			tmp = NEXT(tmp);
		}
	}
	if (tmp)
	{
		if (MAGIC2(p) != MAGIC)
			fprintf(logfile,"Block of size %d allocated at %s, line %d, freed at %s, line %d, has been overrun\n",
				NBYTES(p), FILE(p), LINE(p), f, l);
		/* Unlink from chain */
		if (last) SET_NEXT(last, NEXT(p));
		else heap_list_head = NEXT(p);
		/* save who freed */
		SET_FILE(p, store_name(f));
		SET_LINE(p,l);
		/* Fill with garbage - not implemented yet */
		/* Do the actual free */
		free(((char *)p) - 5 * sizeof(long));
	}
	else goto error; /* didn't find it */
	mem_alloc -= NBYTES(p);
	free_cnt++;
	return;
error:
#ifdef __MSDOS__
	/* Causes seg violation on UNIX */
	if (MAGIC1(p)==MAGIC)
	{
		if (!done_shutdown || strcmp(FILE(p),__FILE__)!=0)
		{
			fprintf(logfile,"Bad call to free from file %s, line %d\n",f,l);
			fprintf(logfile,"Apparently freed before at %s, line %d, size %d\n", FILE(p), LINE(p), NBYTES(p));
		}
	}
	else
#endif
	if (!done_shutdown)
		fprintf(logfile,"Bad call to free from file %s, line %d\n",f,l);
}

static int my_sizehint(void *p, int size)
{
	void *tmp;
#ifdef __MSDOS__
	if (MAGIC1(p)==MAGIC)
#else
	/* Search the list for the block. We don't just check for the magic
		number under UNIX as this can cause a segmentation violation */
	tmp = heap_list_head;
	while (tmp)
	{
		if ( ((void *)tmp) == ((void *)p) ) break;
		assert(MAGIC1(tmp) == MAGIC);
		tmp = NEXT(tmp);
	}
	if (tmp)
#endif
	{
		assert(size == sizeof(char *));
		return NBYTES(p);
	}
	else if (size == sizeof(char *))
		return -1; /* no clues */
#ifdef OLDSTRINGS
	else return size;
#else
	else return -1;	/* 'cause we're fooled by the data[1] def */
#endif

}

/*****************************/
/* String function debugging */
/*****************************/

/* Validate a single pointer arg */

static int my_validate1(char *name, char *s, char *f, int l)
{
	if (s==NULL)
		fprintf(logfile,"%s(NULL) at file %s, line %d\n", name, f, l);
	else return 0;
	return -1;
}

/* Validate a pair of pointer args */

static int my_validate2(char *name, char *s1, char *s2, char *f, int l)
{
	/* Just validate arguments */
	if (s1==NULL && s2==NULL)
		fprintf(logfile,"%s(NULL,NULL) at file %s, line %d\n", name, f, l);
	else if (s1==NULL)
		fprintf(logfile,"%s(NULL,...) at file %s, line %d\n", name, f, l);
	else if (s2==NULL)
		fprintf(logfile,"%s(...,NULL) at file %s, line %d\n", name, f, l);
	else return 0;
	return -1;
}

/* Handler for strcpy, stpcpy, strncpy, strcat, strncat, memcpy,
	and memmove */

char *my_memcpy(char *d, int space_avail, int limit, char *s, int flag, char *f, int l)
{
	char *rtn = d;
	int space_needed, i, dlen;
	if (!logfile) my_initialise();
	if (my_validate2("String/mem copy", s,d,f,l)) return d;
	/* how much space do we need? */
	space_needed = limit ? limit : (strlen(s)+1);
	space_avail = my_sizehint(d, space_avail);
	/* Are we cat'ing? If so, how long is the existing stuff? */
	if (flag&4)
	{
		dlen = strlen(d);
		if (space_avail > 0 && dlen > space_avail)
		{
			fprintf(logfile,"strcat overrun at file %s, line %d - truncating\n\ttarget length %d (used %d), source length %d\n",
				f, l, space_avail, dlen, space_needed);
		}
	} else dlen = 0;
	/* space enuf? */
	if (space_avail >= 0 && space_needed > (space_avail-dlen))
	{
		fprintf(logfile,"String/memory copy overrun at file %s, line %d - truncating\n\ttarget space %d, source length %d\n",
			f, l, (space_avail-dlen), space_needed);
		limit = space_avail-dlen;
	}
	else limit = space_needed;
	d += dlen;
	if (s<d && (s+limit)>=d && (flag&1)==0) /* forward copy would clobber source */
		fprintf(logfile,"String/memory copy clobber in %s, line %d\n", f, l);
	memmove(d,s,limit);
	return rtn + ( (flag & 2) ? (limit-1) : 0); /* hax for stpcpy */
}

char *my_memset(char *d, int space_avail, int limit, char c, char *f, int l)
{
	char *rtn = d;
	int space_needed, i;
	if (!logfile) my_initialise();
	if (my_validate1("memset", d, f, l)) goto done;
	/* how much space do we have? */
	space_avail = my_sizehint(d, space_avail);
	/* space enuf? */
	if (space_avail >= 0 && limit > space_avail)
	{
		fprintf(logfile,"memset overrun at file %s, line %d - truncating\n\ttarget length %d, source length %d\n",
			f, l, space_avail, limit);
		limit = space_avail;
	}
	memset(d,limit,c);
done:
	return rtn;
}

int my_memcmp(char *s1, char *s2, int n, int flag, char *f, int l)
{
	if (!logfile) my_initialise();
	/* Just validate arguments */
	if (my_validate2("memcmp", s1, s2, f, l))
		return ((s1?1:0)-(s2?1:0));
	if (n==0) return strcmp(s1,s2);
	else if (flag) return strncmp(s1,s2,n);
	else return memcmp(s1,s2,n);
}

int my_strlen(char *s, char *f, int l)
{
	if (!logfile) my_initialise();
	return my_validate1("strlen", s, f, l) ? 0 : strlen(s);
}

char *my_strdup(char *s, char *f, int l)
{
	if (!logfile) my_initialise();
	if (my_validate1("strdup", s, f, l)==0)
	{
		int n = strlen(s)+1;
		char *rtn = (char *)my_calloc(n,f,l);
		assert(rtn);
		my_memcpy(rtn, n, 0, s, 0, f, l);
		return rtn;
	}
	return NULL;
}

char *my_strstr(char *s1, char *s2, char *f, int l)
{
	if (!logfile) my_initialise();
	return my_validate2("strstr", s1, s2, f, l) ? NULL : strstr(s1,s2);
}

char *my_strpbrk(char *s1, char *s2, char *f, int l)
{
	if (!logfile) my_initialise();
	return my_validate2("strpbrk", s1, s2, f, l)  ? NULL : strpbrk(s1,s2);
}

char *my_strchr(char *s, int c, char *f, int l)
{
	if (!logfile) my_initialise();
	return my_validate1("strchr", s, f, l) ? NULL : strchr(s,c);
}

char *my_strrchr(char *s, int c, char *f, int l)
{
	if (!logfile) my_initialise();
	return my_validate1("strrchr", s, f, l) ? NULL : strrchr(s,c);
}

int my_strspn(char *s1, char *s2, char *f, int l)
{
	if (!logfile) my_initialise();
	return my_validate2("strspn", s1, s2, f, l) ? 0 : strspn(s1,s2);
}

int my_strcspn(char *s1, char *s2, char *f, int l)
{
	if (!logfile) my_initialise();
	return my_validate2("strcspn", s1, s2, f, l) ? 0 : strcspn(s1,s2);
}

/***************************/
/* File function debugging */
/***************************/

#ifndef MAX_FILES
#define MAX_FILES		64
#endif

typedef struct
{
	char *name;
	char *fname;
	int line;
	FILE *fp;
} file_info_t;

static file_info_t file_info[MAX_FILES];

void my_file_initialise(void)
{
	int i;
	for (i=0;i<MAX_FILES;i++)
	{
		file_info[i].name = NULL;
		file_info[i].fname = NULL;
		file_info[i].line = 0;
		file_info[i].fp = NULL;
	}
}

#include <fcntl.h>

FILE *my_fopen(char *n, char *m, char *f, int l)
{
	FILE *rtn;
	if (!logfile) my_initialise();
	rtn = fopen(n,m);
	if (rtn)
	{
		int h = fileno(rtn);
		if (file_info[h].line>0)
			/* shouldn't happen unless system fopen is broken */
			fprintf(logfile,"%2d (%s) fopened at %s, line %d was already opened at %s, line %d\n",
				h, n, f, l, file_info[h].fname, file_info[h].line);
#ifdef GW_TRACE
		else
			fprintf(logfile,"File %2d (%s) fopened at %s, line %d\n", h, n, f, l);
#endif
		file_info[h].name = store_name(n);
		file_info[h].fname = store_name(f);
		file_info[h].line = l;
		file_info[h].fp = rtn;
	}
	else
		fprintf(logfile,"fopen of %s at %s, line %d failed!\n", n, f, l);
	return rtn;
}

int my_fclose(FILE *fp, char *f, int l)
{
	if (!logfile) my_initialise();
	if (fp)
	{
		int h = fileno(fp);
		if (file_info[h].line <= 0)
			fprintf(logfile,"Bad close(%d) at %s, line %d; already closed at %s, line %d\n",
				h, f, l, file_info[h].fname, -file_info[h].line);
		else
		{
#ifdef GW_TRACE
			fprintf(logfile,"File %2d (%s) opened at %s, line %d fclosed at %s, line %d\n",
						h, file_info[h].name, file_info[h].fname,
						file_info[h].line, f, l);
#endif
			file_info[h].fname = store_name(f);
			file_info[h].line = -l;
			return fclose(fp);
		}
	} else
		fprintf(logfile,"Illegal fclose(NULL) by %s line %d\n",
			 f, l);
	return 0;
}

int my_open(char *n, int m, int a, char *f, int l)
{
	int rtn;
	if (!logfile) my_initialise();
	rtn = open(n,m,a);
	if (rtn >= 0)
	{
		if (file_info[rtn].line>0)
			/* shouldn't happen unless system fopen is broken */
			fprintf(logfile,"%2d (%s) fopened at %s, line %d was already open at %s, line %d\n",
				rtn, n, f, l, file_info[rtn].fname, file_info[rtn].line);
#ifdef GW_TRACE
		else
			fprintf(logfile,"File %2d (%s) fopened at %s, line %d\n", rtn, n, f, l);
#endif
		file_info[rtn].name = store_name(n);
		file_info[rtn].fname = store_name(f);
		file_info[rtn].line = l;
		file_info[rtn].fp = NULL;
	}
	else
		fprintf(logfile,"open of %s at %s, line %d failed!\n\t(%s)\n",
			n, f, l, strerror(errno));
	return rtn;
}

int my_close(int h, char *f, int l)
{
	if (!logfile) my_initialise();
	if (h>=0)
	{
		if (file_info[h].line <= 0)
			fprintf(logfile,"Bad close(%d) by %s, line %d; already closed at %s, line %d\n",
				h, f, l, file_info[h].fname, -file_info[h].line);
		else
		{
#ifdef GW_TRACE
			fprintf(logfile,"File %2d (%s) opened at %s, line %d, fclosed at %s, line %d\n",
						h, file_info[h].name, file_info[h].fname,
						file_info[h].line, f, l);
#endif
			file_info[h].fname = store_name(f);
			file_info[h].line = -l;
			return close(h);
		}
	} else
		fprintf(logfile,"Illegal close(%d) by %s line %d\n", h, f, l);
	return 0;
}

int my_dup(int h, char *f, int l)
{
	int rtn = -1;
	if (!logfile) my_initialise();
	if (h>=0)
	{
		if (file_info[h].line <= 0)
			fprintf(logfile,"Illegal dup(%d) by %s line %d\n", h, f, l);
		else
		{
			rtn = dup(h);
			if (rtn >= 0)
			{
#ifdef GW_TRACE
				fprintf(logfile,"File %2d (%s) opened at %s, line %d, dup'ed to %d at %s, line %d\n",
					h,
					file_info[h].name,
					file_info[h].fname,
					file_info[h].line,
					rtn, f, l);
#endif
				file_info[rtn].name = file_info[h].name;
				file_info[rtn].fname = store_name(f);
				file_info[rtn].line = l;
				file_info[rtn].fp = NULL;
			}
			else
				fprintf(logfile,"dup(%d) by %s line %d failed!\n\t(%s)\n",
					h, f, l, strerror(errno));
		}
	}
	else
		fprintf(logfile,"Illegal dup(%d) by %s line %d!\n", h, f, l);
	return rtn;
}

static int my_readcheck(char *name, void *buf, unsigned len, int space_avail, char *f, int l)
{
	if (buf==NULL)
	{
		fprintf(logfile,"%s with NULL buffer by %s line %d!\n", name, f, l);
		return 0;
	}
	space_avail = my_sizehint(buf, space_avail);
	if (space_avail >= 0 && space_avail < len)
	{
		fprintf(logfile,"%s with count (%d)  > available space (5d) by %s line %d!\n",
			name, len, space_avail, f, l);
		return space_avail;
	}
	else return len;
}

int my_read(int h, void *buf, unsigned len, int space_avail, char *f, int l)
{
	if (!logfile) my_initialise();
	len = my_readcheck("read", buf, len, space_avail, f, l);
	return len ? read(h,buf,len) : 0;
}

size_t my_fread(void *buf, size_t size, size_t n, FILE *fp, int space_avail, char *f, int l)
{
	if (!logfile) my_initialise();
	if (size==0)
	{
		fprintf(logfile,"fread with size zero at %s line %d!\n", f, l);
		return 0;
	}
	n = my_readcheck("fread", buf, n*size, space_avail, f, l) / size;
	return n ? fread(buf,size,n,fp) : 0;
}

char *my_fgets(void *buf, int n, FILE *fp, int space_avail, char *f, int l)
{
	if (!logfile) my_initialise();
	n = my_readcheck("fgets", buf, n, space_avail, f, l);
	return n ? fgets((char *)buf,n,fp) : (char *)buf;
}

static void my_file_report(int is_last)
{
	int i, done_heading=0;
	for (i=0;i<40;i++)
		if (file_info[i].line>0)
		{
			if (is_last && !done_heading)
			{
				done_heading = 1;
				fprintf(logfile,"FILE LEAKS:\n");
			}
			fprintf(logfile,"\tFile `%s' (handle %d) opened at %s, line %d\n",
				file_info[i].name,i,file_info[i].fname,	file_info[i].line);
		}
}

/*************************/
/* Building on the past! */
/*************************/

void *my_realloc(void *p, size_t n, char *f, int l)
{
	void *rtn = my_calloc(n,f,l);
	if (p)
	{
		my_memcpy((char *)rtn, n, 0, (char *)p, 0, f, l);
		my_free(p,f,l);
	}
	return rtn;
}

/**********************/
/* Managed name store */
/**********************/

#ifndef MAX_NAMES
#define MAX_NAMES	32
#endif

static char *file_names[MAX_NAMES];
static int filename_index;

static char *store_name(char *name)
{
	int filename_index;
	for (filename_index=0 ; filename_index<MAX_NAMES ; filename_index++)
	{
		if (file_names[filename_index]==NULL)
		{
			file_names[filename_index] = (char *)malloc(strlen(name)+1);
			assert(file_names[filename_index]);
			strcpy(file_names[filename_index],name);
			return file_names[filename_index];
		} else if (strcmp(file_names[filename_index],name)==0)
			return file_names[filename_index];
	}
	return NULL;
}

static void my_name_initialise(void)
{
	int j;
	for (j=0 ; j<MAX_NAMES ; j++)
		file_names[j]=NULL;
	filename_index = 0;
}

/*****************************/
/* First-time initialisation */
/*****************************/

void my_initialise(void)
{
	my_name_initialise();
	my_memory_initialise();
	my_file_initialise();
	atexit(my_report);
#ifdef DEBUG_LOG
	logfile = fopen(DEBUG_LOG,"w");
	assert(logfile);
#else
	logfile = stderr;
#endif
}

/********************************/
/* Generate a report at the end */
/********************************/

void my_report(void)
{
	time_t tm = time(NULL);
	if (!logfile) my_initialise();
	fprintf(logfile,"\n\n================ END-OF-PROGRAM DEBUG LOG ===================\n");
	fprintf(logfile,"Log date: %s\n\n", ctime(&tm));
	my_memory_report(1);
	fprintf(logfile,"\n\n");
	my_file_report(1);
	fprintf(logfile,"\n\n================ END OF LOG ===================\n\n");
#ifdef DEBUG_LOG
	fclose(logfile);
#endif
	logfile=NULL;
}

/*===================================================================*/

#endif /* GW_DEBUG */

