#include "tcl.h"

#if __MSDOS__

#define	S_ISDIR( mode )		((mode) & S_IFDIR)
#define	S_ISCHR( mode )		((mode) & S_IFCHR)
#define	S_ISBLK( mode )		((mode) & S_IFBLK)
#define	S_ISFIFO( mode )	((mode) & S_IFIFO)
#define	S_ISREG( mode )		(((mode) & 0x7000)==0)

#endif

static struct stat st;

static inline struct stat *Stat(char *pathname)
{
    return (stat(pathname, &st) == 0) ? &st : NULL;
}

static inline struct stat *LStat(char *pathname)
{
#if defined(NO_SYMLINKS) || __MSDOS__
    return (stat(pathname, &st) == 0) ? &st : NULL;
#else
    return (lstat(pathname, &st) == 0) ? &st : NULL;
#endif
}

#if __MSDOS__
typedef unsigned short mode_t;
typedef long	uid_t;
typedef long	gid_t;
typedef long	off_t;
typedef long	time_t;
#endif

static inline off_t FileSize(char *pathname)
{
    struct stat *st = Stat(pathname);
    return st ? st->st_size : 0l;
}

static inline mode_t FileMode(char *pathname)
{
    struct stat *st = Stat(pathname);
    return st ? st->st_mode : 0;
}

static inline gid_t FileGroup(char *pathname)
{
    struct stat *st = Stat(pathname);
    return st ? st->st_gid : 0;
}

static inline uid_t FileUser(char *pathname)
{
    struct stat *st = Stat(pathname);
    return st ? st->st_uid : 0;
}

static inline time_t FileModTime(char *pathname)
{
    struct stat *st = Stat(pathname);
    return st ? st->st_mtime : 0;
}

static inline time_t FileAccTime(char *pathname)
{
    struct stat *st = Stat(pathname);
    return st ? st->st_atime : 0;
}

/***************************************************************
		DATE FIXER NEEDED FOR BORLAND C
****************************************************************/

#ifdef __MSDOS__
static time_t normaliseDate(unsigned yr, unsigned mon, unsigned day,
			     unsigned hr, unsigned mins, unsigned sec)
{
    struct date dt;
    struct time tm;
    tm.ti_hour = hr;
    tm.ti_min = mins;
    tm.ti_sec = sec;
    dt.da_year = (yr < 100) ? (1900 + yr) : yr;
    dt.da_mon = mon;
    dt.da_day = day;
    /* Borland's dostounix crashes on bad dates, so we do a reality check */
    if (dt.da_year < 1980) dt.da_year = 1980;
    else if (dt.da_year > 1999) dt.da_year = 1999;
    if (dt.da_mon < 1) dt.da_mon = 1;
    else if (dt.da_mon > 12)	dt.da_mon = 12;
    if (dt.da_day > 31) dt.da_day = 31;
    else if (dt.da_day < 1) dt.da_day = 1;
    if (tm.ti_hour > 23) tm.ti_hour = 23;
    if (tm.ti_min > 59) tm.ti_min = 59;
    if (tm.ti_sec > 59)	tm.ti_sec = 59;
    return (time_t) dostounix(&dt, &tm);
}

#endif

void SaveStatResults(struct stat *st, char *aname)
{
    String tmp;
    tmp = (int)st->st_size;	SetVar(tmp, aname, "size");
    tmp = (int)st->st_mode;	SetVar(tmp, aname, "mode");
    tmp = (int)st->st_gid;	SetVar(tmp, aname, "gid");
    tmp = (int)st->st_uid;	SetVar(tmp, aname, "uid");
    tmp = (int)st->st_mtime;	SetVar(tmp, aname, "mtime");
    tmp = (int)st->st_atime;	SetVar(tmp, aname, "atime");
    tmp = (int)st->st_nlink;	SetVar(tmp, aname, "nlink");
    tmp = (int)st->st_ctime;	SetVar(tmp, aname, "ctime");
    tmp = (int)st->st_dev;	SetVar(tmp, aname, "dev");
    tmp = (int)st->st_ino;	SetVar(tmp, aname, "ino");
}

//	GC Directory Reading

/* Once upon a time this was all simple, using opendir,
   readdir and closedir. Then I decided to handle hidden
   files. Trivial under UNIX, but a pain under DOS. This
   is the mess that resulted....
*/

#ifdef __MSDOS__

static char dirPath[MAXPATH];
static struct ffblk fdat, *fdp = NULL;

int OpenDir(char *pname)
{
    assert(fdp==NULL);
    if (!S_ISDIR(FileMode(pname))) return -1;
    strcpy(dirPath, pname);
    if (dirPath[strlen(dirPath)-1]!='\\')
    	strcat(dirPath,"\\*.*");
    else strcat(dirPath,"*.*");
    return 0; // non-null
}

char *ReadDirEntry(int hidden)
{
    if (fdp==NULL)
    {
	fdp = &fdat;
    	if (findfirst(dirPath, fdp,
    		FA_RDONLY|FA_DIREC|FA_ARCH|(hidden ? FA_HIDDEN : 0)))
    			return NULL;
    }
    else if (findnext(fdp))
	return NULL;
    return fdp->ff_name;
}

void CloseDir()
{
    fdp = NULL;
}

#else
//------------------
#  ifdef NO_DIRENT_H

static int dirid = -1;

int OpenDir(char *pname)
{
    assert(dirid<0);
    dirid = open(pname,0,0);
    return (dirid<0) ? -1 : 0 ;
}

char *ReadDirEntry(int hidden)
{
    static struct direct de;
    if (dirid>=0)
    {
    	while (read(dirid,&de,sizeof(de))==sizeof(de))
    	{
    	    if (!hidden && de.d_name[0]=='.'
    	    	&& strcmp(de.d_name,"..")!=0)
    	    	    continue;
    	    else return de.d_name;
    	}
    }
    return NULL;
}

void CloseDir()
{
    if (dirid>=0) close(dirid);
    dirid = -1;
}

#  else
//-----------------------

static DIR *startDir = NULL;

int OpenDir(char *pname)
{
    assert(startDir == NULL);
    startDir = opendir(pname);
    return startDir ? 0 : -1;
}

char *ReadDirEntry(int hidden)
{
    struct dirent *de;
    if (startDir)
    {
    	while ((de=(struct dirent *)readdir(startDir))!=NULL)
    	{
    	    if (!hidden && de->d_name[0]=='.'
    	    	&& strcmp(de->d_name,"..")!=0)
    	    	    continue;
    	    else return de->d_name;
    	}
    }
    return NULL;
}

void CloseDir()
{
    if (startDir) closedir(startDir);
    startDir = NULL;
}

#  endif
#endif

//-----------------------------------------------------------
// Handlers

int cdH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
#if __MSDOS__
    if (args.Count() != 2)
#else
    if (args.Count()<1 || args.Count()>2)
#endif
    {
	result = "wrong # of args in cd";
	rtn = ERROR;
    }
    else
    {
#if __MSDOS__
	if (args.Count()==2)
	{
	    // Change DOS drive
	    if (args.GetString(1)[1]==':')
	    {
		int drv = args.GetString(1)[0];
		if (drv>='a') drv -= 'a';
		else drv -= 'A';
		setdisk(drv);
		if (getdisk() != drv)
		    rtn = ERROR;
	    }
	    if (rtn == OKAY)
	    {
		if (chdir(args.GetString(1))!=0)
		    rtn = ERROR;
	    }
	    if (rtn == ERROR)
	    {
		result = "Failed to change to directory ";
		result += args.GetString(1);
	    }
	}
#else
	char *dir;
	if (args.Count()>1) dir = args.GetString(1);
	else dir = getenv("HOME");
	(void)chdir(dir);
#endif
    }
    return rtn;
}

int closeH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    FILE *fp = FindFile(args.GetString(1));
    if (fp==NULL)
    {
        result = "bad file ID in close";
        rtn = ERROR;
    }
    else
    {
	fclose(fp);
	DeleteFile(args.GetString(1));
    }
    return rtn;
}

int eofH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    FILE *fp = FindFile(args.GetString(1));
    if (fp==NULL)
    {
        result = "bad file ID in eof";
        rtn = ERROR;
    }
    else
	result = (int)feof(fp);
    return rtn;
}

static char *file_opts[] =
{
    "atime",		// 0
    "dirname",
    "executable",
    "exists",
    "extension",
    "isdirectory",	// 5
    "isfile",
    "lstat",
    "mtime",
    "owned",
    "readable",		// 10
    "readlink",
    "rootname",
    "size",
    "stat",
    "tail",		// 15
    "type",
    "writable"
};

int fileH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY, na = args.Count();
    int opt = (na < 2 ) ? -1 :
		FindString(args.GetString(1), file_opts, 
			sizeof(file_opts)/sizeof(file_opts[0]));
    int ac = (opt == 7 || opt == 14) ? 4 : 3;
    char *fname = (char *)args.GetString(2), *fptr;
    mode_t md;
    time_t tm;
    struct stat *st;
    if (opt < 0 || args.Count() != ac)
    {
        result = "wrong # args or bad args in file";
	rtn = ERROR;
    }
    else switch (opt)
    {
    case 0:	// atime
	tm = FileAccTime(fname);
	result = ctime(&tm);
	break;
    case 1:	// dirname
#if __MSDOS__
	result = fname;
	if ((fptr = strrchr(fname, PATH_SEP))==NULL)
	{
	    if (fname[2]==':')
	    {
		result.Trunc(2);
	        result = (char)'.';
	    }
	    else result = (char)'.';
	}
	else if ((fptr-fname)==0)
	    result.Trunc(1);
	else if (fname[2]==':' && (fptr-fname)==2)
	    result.Trunc(3);
	else
	    result.Trunc(fptr-fname);
#else
	if ((fptr = strrchr(fname, PATH_SEP))==NULL)
	    result = (char)'.';
	else if (fptr == fname)
	    result = (char)'/';
	else
	{
	    result = fname;
	    result.Trunc(fptr-fname);
	}
#endif
	break;
    case 2:	// executable
#if __MSDOS__
	result = (int)((FileMode(fname) & S_IEXEC) != 0);
#else
	result = (int)((FileMode(fname) & S_IXUSR) != 0);
#endif
	break;
    case 3:	// exists
	result = (int)(access(fname, 0)==0);
	break;
    case 4:	// extension
	fptr = strrchr(fname, '.');
	if (fptr)
	{
	    char *sptr = strrchr(fname, PATH_SEP);
	    if (sptr == NULL || sptr < fptr)
		result = fptr;
	}
	break;
    case 5:	// isdirectory
	result = (int)(S_ISDIR(FileMode(fname)));
	break;
    case 6:	// isfile
	result = (int)(S_ISREG(FileMode(fname)));
	break;
    case 7:	// lstat
	st = Stat(fname);
	SaveStatResults(st, args.GetString(3));
	break;
    case 8:	// mtime
	tm = FileModTime(fname);
	result = ctime(&tm);
	break;
    case 9:	// owned
#if __MSDOS__
	result = (int)1;
#else
	result = (int) (FileUser(fname) == getuid());
#endif
	break;
    case 10:	// readable
#if __MSDOS__
	result = (int)((FileMode(fname) & S_IREAD) != 0);
#else
	result = (int)((FileMode(fname) & S_IRUSR) != 0);
#endif
	break;
    case 11:	// readlink!!
	break;
    case 12:	// rootname
	result = fname;
	fptr = strrchr(fname, '.');
	if (fptr)
	{
	    char *sptr = strrchr(fname, PATH_SEP);
	    if (sptr==NULL || sptr < fptr)
		result.Trunc(fptr-fname);
	}
	break;
    case 13:	// size
	result = (int)(FileSize(fname));
	break;
    case 14:	// stat
	st = Stat(fname);
	SaveStatResults(st, args.GetString(3));
	break;
    case 15:	// tail
	result = fname;
	fptr = strrchr(fname, PATH_SEP);
	if (fptr)
	    result = fptr+1;
	break;
    case 16:	// type
	md = FileMode(fname);
#if !__MSDOS__
	if (S_ISLNK(md))
	    result = "link";
	else
#endif
	if (S_ISDIR(md))
	    result = "directory";
	else if (S_ISCHR(md))
	    result = "characterSpecial";
	else if (S_ISBLK(md))
	    result = "blockSpecial";
	else if (S_ISFIFO(md))
	    result = "fifo";
//	else if (S_IS(md))
//	    result = "socket";
	else
	    result = "file";
	break;
    case 17:	// writable
#if __MSDOS__
	result = (int)((FileMode(fname) & S_IWRITE) != 0);
#else
	result = (int)((FileMode(fname) & S_IWUSR) != 0);
#endif
	break;
    }
    return rtn;
}


int flushH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    FILE *fp = FindFile(args.GetString(1));
    if (fp==NULL)
    {
        result = "bad file ID in flush";
        rtn = ERROR;
    }
    else
    {
	result = "";
	fflush(fp);
    }
    return rtn;
}

int getsH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    if (args.Count()<2 || args.Count()>3)
    {
        result = "wrong # args in gets";
	rtn = ERROR;
    }
    else
    {
	FILE *fp = FindFile(args.GetString(1));
	if (fp == NULL)
	{
            result = "bad file ID in gets";
            rtn = ERROR;
	}
	else
        {
	    String input;
	    while (!feof(fp))
	    {
	        char buff[512];
	        fgets(buff, 512, fp);
		int cnt = strlen(buff);
		if (cnt>0 && buff[cnt-1]=='\n')
		{
		    buff[cnt-1] = 0;
		    input += buff;
		    break;
		}
		else input += buff; // got more to come
	    }
	    if (args.Count()==3)
	    {
		String *base, *idx;
		char *i = SplitVar(args.GetString(2), base, idx);
 	    	SetVar(input, (char *)*base, i);
		if (base) delete base;
		if (idx) delete idx;
		if (input.Length()==0 && feof(fp))
		    result = -1;
		else
		    result = input.Length();
	    }
	    else
	        result = input;
        }
    }
    return rtn;
}

int openH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    if (args.Count()!=2 && args.Count()!=3)
    {
        result = "wrong # args in open";
	rtn = ERROR;
    }
    else
    {
        char *perms = (args.Count()==3) ? args.GetString(2) : "r";
        FILE *fp = fopen((char *)args.GetString(1), perms);
	if (fp == NULL)
	{
            result = "open of ";
            result += args.GetString(1);
            result = " failed";
	    rtn = ERROR;
	}
	else result = RecordFile(fp);
    }
    return rtn;
}

int putsH(StringGroup &args, String &result)
{
    TraceThis();
    int rtn = OKAY;
    if (args.Count()<2 || args.Count()>4)
    {
        result = "wrong # args in puts";
	rtn = ERROR;
    }
    else
    {
        FILE *fp = stdout;
        int na = args.Count();
        int addnl = (strcmp((char *)args.GetString(1), "-nonewline")!=0);
        int fid_arg = (addnl ? 1 : 2);
        if (fid_arg == (na-2)) // hax; true if file id present
        {
	    fp = FindFile(args.GetString(na-2));
	    if (fp == NULL)
	    {
                result = "bad file ID in puts";
                rtn = ERROR;
	    }
        }
        if (fp)
        {
	    fputs(args.GetString(na-1), fp);
	    if (addnl) fputs("\n", fp);
        }
    }
    return rtn;
}

int globH(StringGroup &args, String &result)
{
#if __MSDOS__
    char buff[MAXPATH];
#endif
    char *nm;
    int na = args.Count(), cnt=0;
    for (int a = 1; a < na; a++)
    {
	int dironly = 0, incurrent = 0;
	String arg(args.GetString(a));
	if (arg[arg.Length()-1]==PATH_SEP)
	{
	    arg.Trunc(arg.Length()-1);
	    dironly = 1;
	}
	char *pstr = arg, *pat = strrchr(pstr, PATH_SEP), *dir;
	if (pat == NULL)
	{
#if __MSDOS__
    	    dir = getcwd(buff, 256);
#else
	    dir = ".";
#endif
	    pat = pstr;
	    incurrent = 1;
	}
	else
	{
	    dir = pstr;
	    *pat++ = 0;
	}
	SetGlobPattern(pat);
        (void)OpenDir(dir);
        while ((nm = ReadDirEntry(1)) != NULL)
        {
	    if (GlobMatch(nm))
	    {
		String nmstr;
		if (!incurrent)
		{
		    nmstr = dir;
		    nmstr += (char)PATH_SEP;
		}
		nmstr += nm;
		if (!dironly || S_ISDIR(FileMode((char *)nmstr)))
		{
		    if (cnt++) result += (char)' ';
		    result += nmstr;
		}
	    }
	}
	CloseDir();
    }
    return OKAY;
}


