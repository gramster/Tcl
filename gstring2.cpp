#include <assert.h>
#include "gstring.h"
#include "gwdebug.h"

#ifdef GW_DEBUG
#define ALLOC(len)	(char *)malloc(len)
#define FREE(t)		free(t)
#else
#define ALLOC(len)	new char [len]
#define FREE(t)		delete [] t
#endif

void String::Set(const char *text_in)
{
    if (text) FREE(text);
    len = text_in ? strlen((char *)text_in) : 0;
    text = ALLOC(len+1);
    if (text_in) strcpy(text, (char *)text_in);
    else text[0]=0;
}

void String::Append(const char *text_in)
{
    if (text_in && text_in[0])
    {
    	char *oldtext = text;
    	len += strlen((char *)text_in);
    	text = ALLOC(len+1);
    	strcpy(text, (char *)oldtext);
    	strcat(text, (char *)text_in);
    	FREE(oldtext);
    }
}

String::String(const char *text_in)
{
    text = NULL;
    Set(text_in);
}

String::String(const String &s_in)
{
    text = NULL;
    Set(s_in.text);
}

int String::Length()
{
    return len;
}

String::~String()
{
    FREE(text);
}

String &String::operator=(const char *text_in)
{
    Set(text_in);
    return *this;
}

String &String::operator=(const String &s_in)
{
    Set(s_in.text);
    return *this;
}

char &String::operator[](unsigned i)
{
    assert(i<len);
    return text[i];
}

void String::operator+=(const char *text_in)
{
    Append(text_in);
}

void String::operator+=(const String &s_in)
{
    Append(s_in.text);
}

void String::operator+=(const char c)
{
    char s[2];
    s[0] = c;
    s[1] = 0;
    Append(s);
}

String operator+(const String &s1, const String &s2)
{
    String rtn(s1);
    rtn += s2;
    return rtn;
}

int Compare(const String &s1, const String &s2)
{
    return strcmp(s1.text, s2.text);
}

int operator==(const String &s1, const String &s2)
{
    return Compare(s1,s2)==0;
}

int operator!=(const String &s1, const String &s2)    
{
    return Compare(s1,s2)!=0;
}

int operator>(const String &s1, const String &s2)    
{
    return Compare(s1,s2)>0;
}

int operator>=(const String &s1, const String &s2)    
{
    return Compare(s1,s2)>=0;
}

int operator<(const String &s1, const String &s2)    
{
    return Compare(s1,s2)<0;
}

int operator<=(const String &s1, const String &s2)
{
    return Compare(s1,s2)<=0;
}

#ifdef TEST

/* Simple test code */

#include <stdio.h>

int main()
{
    String a("the cat "), b("sat on the mat");
    puts(a);
    puts(b);
    a += b;
    puts(a);
    String c = a + b;
    puts(c);
    puts(b+b);
}

#endif

