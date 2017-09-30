//--------------------------------------------------
// Window contents lists. Used for window views

#ifndef __LISTS_H
#define __LISTS_H

// virtual base for each line

class Line
{
    Line *next, *prev;
public:
    Line()		{ next = prev = NULL;	}
    Line *Next()	{ return next;		}
    Line *Prev()	{ return prev;		}
    void Next(Line *n)	{ next = n;		}
    void Prev(Line *p)	{ prev = p;		} 

    virtual char *Text() { return NULL;		}
    virtual void Mark(int m) { (void)m;		}
    virtual int Marked() { return 0;		}
    virtual int Compare(Line *wl, int key) { return 0; }
    virtual void Swap(Line *wl) {}
};

class FileLine : public Line
{
    char name[20]; // for now;
    int marked;
public:
    FileLine(char *nm)
	: Line()
    {
	strcpy(name, nm);
	marked = 0;
    }
    virtual char *Text()
    {
	return name;
    }
    virtual void Mark(int m)
    {
	marked = m;
    }
    virtual int Marked()
    {
	return marked;
    }
    virtual int Compare(Line *wl, int key)
    {
	return strcmp(name, ((FileLine *)wl)->name);
    }
    virtual void Swap(Line *wl);
};

class LineList
{
    Line head, *now;
    int cnt, pos;
    void Init();
public:
    LineList();
    ~LineList();
    void Sort(int key);
    void Add(Line *l);
    void Remove(Line *l);
    void Goto(int n);
    int Find(char *name);
    void Empty();

    void Forward(int n = 1)	{ Goto(pos+n);		}
    void Back(int n = 1)	{ Goto(pos-n);		}
    void Head()			{ Goto(0);		}
    void Tail()			{ Goto(cnt-1);		}
    char *Text()		{ return now?now->Text():NULL;		}
    void Mark(int m)		{ assert(now); now->Mark(m);		}
    int Marked()		{ assert(now); return now->Marked();	}
    int Count()			{ return cnt;   	}
    int Pos()			{ return pos;   	}
};

inline void FileLine::Swap(Line *wl)
{
    char nm[14];
    int om = marked;
    marked = ((FileLine *)wl)->marked;
    ((FileLine *)wl)->marked = om;
    strcpy(nm, name);
    strcpy(name, ((FileLine *)wl)->name);
    strcpy(((FileLine *)wl)->name, nm);
}

inline void LineList::Init()
{
    now = NULL;
    head.Next(&head);
    head.Prev(&head);
    cnt = pos = 0;
}

inline LineList::LineList()
{
    Init();
}

inline LineList::~LineList()
{
    Empty();
}

inline void LineList::Sort(int key)
{
}

inline void LineList::Add(Line *l)
{
    l->Next(&head);
    l->Prev(head.Prev());
    head.Prev()->Next(l);
    head.Prev(l);
    cnt++;
}

inline void LineList::Remove(Line *l)
{
    if (now==l) Goto(pos+1);
    l->Prev()->Next(l->Next());
    l->Next()->Prev(l->Prev());
    delete l;
    cnt--;
    pos--;
}

inline int LineList::Find(char *name)
{
    return 0; // 4 now
}

#endif

