#ifndef __GSTRING_H
#define __GSTRING_H

#include "tcltop.h"

//------------------------------------------------------------

#ifdef DEBUG
extern long strcount;

#define IncCnt()	strcount++
#define DecCnt()	strcount--
#else
#define IncCnt()
#define DecCnt()
#endif

#define SMALLSTRING	16 	// Must be a power of 2!!
#define STRINGMASK	(~15) 	// high bits of size

class String
{
protected:
    char	data[SMALLSTRING];
    char	*text;
    unsigned	len;
    unsigned	sz;

    void Expand(int newsz, int keep);
    void Resize(int length, int keep = 0)
    {
        TraceThis();
	// round up to next smallstring chunk
        int newsz = SMALLSTRING + (length & STRINGMASK);
        if (newsz != sz)
	    Expand(newsz, keep);
        len = length;
    }
    void Construct()
    {
	TraceThis(); ShowDbgStk("Constructing string %lx", this);
	text = data;
	len = 0;
	sz = SMALLSTRING;
	text[0] = 0;
        IncCnt();
    }
public:
    String()
    {
	Construct();
    }
    String(const char *s)
    {
	Construct();
	if (s)
	{
	    Resize(strlen((char *)s));
	    strcpy(text, (char *)s);
	}
    }
    String(const String &s)
    {
	Construct();
	Resize(s.len);
	strcpy(text, (char *)s.text);
    }
    ~String()
    {
	TraceThis(); ShowDbgStk("Destructing string %lx", this);
	if (sz>SMALLSTRING)
	    delete [] text;
        DecCnt();
    }
    void Trunc(int newlen)
    {
	TraceThis();
	assert(newlen <= len);
	text[newlen] = 0;
	len = newlen;
    }
    const int Length() const
    {
	return len;
    }
protected:
    void SetStr(const char *s, int length = -1)
    {
	TraceThis();
	if (length < 0) length = strlen((char*)s);
	(void)Resize(length, 0);
	strcpy(text, s);
    }
    void SetInt(int val)
    {
	TraceThis();
	char v[16];
	sprintf(v, "%d", val);
	SetStr(v);
    }
    void AppendStr(const char *string, int length)
    {
	TraceThis();
	(void)Resize(len+length, 1);
	strcat(text, string);
    }
    void AppendChar(char ch)
    {
	TraceThis();
	(void)Resize(len+1, 1);
	text[len-1] = ch;
	text[len] = 0;
    }
    void AppendInt(int val)
    {
	TraceThis();
	char v[16];
	sprintf(v, "%d", val);
	AppendStr(v, strlen(v));
    }
public:
    void AppendElement(char *string, int len);
    void AppendElement(String &string)
    {
	TraceThis();
        AppendElement(string.text, string.len);
    }
    void StartSublist()
    {
	TraceThis();
	AppendChar(' ');
	AppendChar('{');
    }
    void EndSublist()
    {
	TraceThis();
	AppendChar('}');
    }
    String &operator=(const char *s_in)
    {
	TraceThis();
	SetStr(s_in, strlen((char *)s_in));
        return *this;
    }
    String &operator=(const String &s_in)
    {
	TraceThis();
	if (this != &s_in)
	    SetStr(s_in.text, s_in.len);
        return *this;
    }
    String &operator=(const char ch)
    {
	TraceThis();
	char str[2];
	str[0] = ch;
	str[1] = 0;
	SetStr(str, 1);
        return *this;
    }
    String &operator=(const int val)
    {
	TraceThis();
	SetInt(val);
        return *this;
    }
    char &operator[](const unsigned i) const
    {
	return text[i];
    }
    void operator+=(const char *text_in)
    {
	TraceThis();
	AppendStr(text_in, strlen(text_in));
    }
    void operator+=(const String &s_in)
    {
	TraceThis();
	AppendStr(s_in.text, s_in.len);
    }
    void operator+=(const char ch)
    {
	TraceThis();
	AppendChar(ch);
    }
    void operator+=(int val)
    {
	TraceThis();
	AppendInt(val);
    }
    const char *Text() const
    {
	return text;
    }
    operator const char*() const
    {
	return text;
    }
    operator char*() const
    {
	return text;
    }
    // things that should really be in a subclass
    int IsNumeric(int *val);
    int IsNumeric(long *val);
};

inline String operator+(const String &s1, const String &s2)
{
    TraceThis();
    String rtn(s1);
    rtn += s2;
    return rtn;
}

inline String operator+(const String &s1, const char* s2)
{
    TraceThis();
    String rtn(s1);
    rtn += s2;
    return rtn;
}

inline String operator+(const String &s1, const char c2)
{
    TraceThis();
    String rtn(s1);
    rtn += c2;
    return rtn;
}

inline String operator+(const char *s1, String &s2)
{
    TraceThis();
    String rtn(s1);
    rtn += s2;
    return rtn;
}

inline String operator+(const char c1, String &s2)
{
    TraceThis();
    String rtn;
    rtn += c1;
    rtn += s2;
    return rtn;
}

inline int Compare(const String &s1, const String &s2)
{
    TraceThis();
    return strcmp((char *)(String &)s1, (char *)(String &)s2);
}

//----------------------------------------------------------------

inline int operator==(const String &a, const String &b)
{
    return (a.Length()==b.Length() && Compare(a, b) == 0);
}

inline int operator!=(const String &a, const String &b)
{
    return (a.Length()!=b.Length() || Compare(a, b) == 0);
}

inline int operator>(const String &a, const String &b)
{
    return Compare(a, b) > 0;
}

inline int operator>=(const String &a, const String &b)
{
    return Compare(a, b) >= 0;
}

inline int operator<(const String &a, const String &b)
{
    return Compare(a, b) < 0;
}

inline int operator<=(const String &a, const String &b)
{
    return Compare(a, b) <= 0;
}

//----------------------------------------------------------------

class InputString : public String
{
    int pos;
public:
    InputString(const char *text_in = NULL)
	: String(text_in)
    {
	TraceThis();
	pos = 0;
    }
    int Next()
    {
        TraceThis();
        if (pos < len)
            return text[pos++];
        else return ETX;
    }
};

class StringGroup
{
protected:
    int elts;
    int size;
    String *str;
public:
    StringGroup()
    {
	TraceThis();
	elts = 0;
	size = 16;
	str = new String[16];
	assert(str);
    }
    virtual ~StringGroup()
    {
	TraceThis();
	delete [] str;
    }
    int Count()
    {
	return elts;
    }
    virtual void Expand(int newsize);
    virtual void SetString(int idx, String &val);
    virtual void SetString(int idx, const char *val);
    virtual String &GetString(int idx)
    {
	TraceThis();
	assert(idx>=0 && idx<elts);
	return str[idx];
    }
    void Dump(String &trace); // for script debugging
};

class SortableStringGroup : public StringGroup
{
    int *idxtbl;
public:
    SortableStringGroup()
	: StringGroup()
    {
	TraceThis();
	idxtbl = new int[16];
	assert(idxtbl);
    }
    virtual ~SortableStringGroup()
    {
	TraceThis();
	delete [] idxtbl;
    }
    virtual void Expand(int newsize);
    virtual void SetString(int idx, String &val)
    {
	StringGroup::SetString(idx, val);
        idxtbl[idx] = idx;
    }
    virtual void SetString(int idx, const char *val)
    {
	StringGroup::SetString(idx, val);
        idxtbl[idx] = idx;
    }
    virtual String &GetString(int idx)
    {
	assert(idx>=0 && idx<elts);
	return str[idxtbl[idx]];
    }
    void Sort(int key, int increasing, char *custom);
};

typedef int (*prochandler_t)(StringGroup &args, String &result);

#endif

    

