#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#define ETX	-1

#ifdef GW_DEBUG
#define ALLOC(len)	(char *)malloc(len)
#define FREE(t)		free(t)
#else
#define ALLOC(len)	new char [len]
#define FREE(t)		delete [] (char *)t
#endif

#define STR_CHUNK	16

class StringText
{
private:
  friend class String;
  unsigned refcnt;
  char data[1];

  void *operator new(size_t n, unsigned d)
  {
      void *p = ALLOC(n+d-1);
      assert(p);
      return p;
  }
  void operator delete(void *p)
  {
      FREE(p);
  }
  StringText()
  {
      refcnt = 1;
  }
};

class String
{
private:
  StringText *value; // shared value
  char *text;      //	start of text
  unsigned size;   //   actual length
  unsigned len;    //   apparent length

  void Alloc(unsigned n)
  {
      value = new(n) StringText; // Allocate string value.
      assert(value);
      text = value->data;    // Point to starting text element
      len = 0;
      size = n;
  }
  void FreeText()		// disassociate string from text
  {
      value->refcnt--;
      if (value->refcnt == 0)
      {
	delete value;
        value = NULL;
      }
  }
  void InitText(const String &s) // associate new string with text
  {
      value = s.value;
      value->refcnt++;
  }
  void SetText(const String &s)	// associate existing string with text
  {
      if (value != s.value)
      {	
          FreeText();
          InitText(s);
      }
  }
  void Construct(const char *s, unsigned n)
  {
      Alloc(n + STR_CHUNK - (n % STR_CHUNK));
      CopyN(s, n);
  }
  friend class InputString;
public:


  String(const String &s, 
         unsigned ofs, unsigned n);
  String &operator=(const String &s);
  void Realloc(unsigned new_size, int keep=1);
  unsigned InsReplAt(unsigned p, const char *s, unsigned n, int ins=1);
  unsigned DeleteAt(unsigned p, unsigned n);
  const char *NullTermStr();

  operator char*() { return (char *)NullTermStr(); } // most important!

  friend int Compare(const String &a, const String &b);

  // inline methods

  String(const String &s)
  {
      InitText(s);
      len = s.len;
      size = s.size;
      text = s.text;
  }
  String(const char *s, unsigned n)
  {
      Construct(s,n);
  }
  String(const char *s)
  {
      Construct(s,strlen(s));
  }
  ~String()
  {
      FreeText();
  }
  String(unsigned n = STR_CHUNK)
  {
      Alloc(n);
  }
  void EnsureUnique()
  {
      if (value->refcnt != 1)
      {
          String &c = Clone();
          SetText(c);
          text = c.text;
      }
  }
  String Mid(unsigned p, unsigned n) const
  {
      return String(*this, p, n);
  }
  String Left(unsigned n) const
  {
      return String(*this, 0, n);
  }
  String Right(unsigned n) const
  {
      if (n > len) n = len;
      return String(*this, len-n, n);
  }
  friend String operator+(const String &a, const String &b)
  {
      String r(a.len + b.len);
      r += a;
      r += b;
      return r;
  }
  String Clone() const
  {
      String temp(size);
      temp.CopyN(text, len);
      return temp;
  }
  void CopyN(const char *s, unsigned n)
  {
      len = 0;
      InsReplAt(0, s, n);
  }
  void Copy(const char *s)
  {
      CopyN(s, strlen(s));
  }
  void Copy(const String &s)
  {
      CopyN(s.text, s.len);
  }
  char &operator[](unsigned i)
  {
      assert(i<=len);
      EnsureUnique();
      return text[i];
  }
  const char &operator[](unsigned i) const
  {
      assert(i<=len);
      return text[i];
  }
  unsigned Size() const
  {
      return size;
  }
  unsigned Length() const
  {
      return len;
  }
  char *Text()
  {
      return text;
  }
  const char *Text() const
  {
      return text;
  }
  unsigned InsertAt(unsigned p, const char *s, unsigned n)
  {
      return InsReplAt(p, s, n);
  }
  unsigned InsertAt(unsigned p, const char *s)
  {
      return InsReplAt(p, s, strlen(s));
  }
  unsigned InsertAt(unsigned p, const String &s)
  {
      return InsReplAt(p, s.text, s.len);
  }
  unsigned ReplaceAt(unsigned p, const char *s, unsigned n)
  {
      return InsReplAt(p, s, n, 0);
  }
  unsigned ReplaceAt(unsigned p, const char *s)
  {
      return InsReplAt(p, s, strlen(s), 0);
  }
  unsigned ReplaceAt(unsigned p, const String &s)
  {
      return InsReplAt(p, s.text, s.len, 0);
  }
  void Concatenate(const char *s, unsigned n)
  {
      InsReplAt(len, s, n);
  }
  String &operator=(const char *s)
  {
      Copy(s);
      return *this;
  }
  void operator+=(const String &s)
  {
      Concatenate(s.text, s.len);
  }
  void operator+=(const char *s)
  {
      Concatenate(s, strlen(s));
  }
  void operator+=(const char c)
  {
      Concatenate(&c, 1);
  }

  // String compares

  friend int operator==(const String &a, const String &b)
  {
      return Compare(a, b) == 0;
  }
  friend int operator!=(const String &a, const String &b)
  {
      return Compare(a, b) != 0;
  }
  friend int operator>(const String &a, const String &b)
  {
      return Compare(a, b) > 0;
  }
  friend int operator>=(const String &a, const String &b)
  {
      return Compare(a, b) >= 0;
  }
  friend int operator<(const String &a, const String &b)
  {
      return Compare(a, b) < 0;
  }
  friend int operator<=(const String &a, const String &b)
  {
      return Compare(a, b) <= 0;
  }
  // things that should really be in a subclass
  void Set(int val);
  int IsNumeric();
  int StripList();
};

// A constructor to create a substring of this string.
// The substring shares it's data with this string,
// starting at offset ofs, and of length n. It has an
// an allocated length of n;

String::String(const String &s, unsigned ofs, unsigned n)
{
  InitText(s);
  // Keep ofs and length in range
  if (ofs > s.len-1) ofs = s.len-1;
  if (n + ofs > s.len) n = s.len-ofs;
  // Set max and logical bounds of the substring
  size = n;
  len = n;
  text = s.text + ofs; // Compute starting text element.
}

String &String::operator=(const String &s)
{
    if (this != &s)
    {
        SetText(s);
        len = s.len;
        size = s.size;
        text = s.text;
    }
    return *this;
}

// Redimensions the string to new_size.
// If keep == 1, data is left intact 
// via copying of elements to new space, otherwise, the
// logical length is set to zero.

void String::Realloc(unsigned new_size, int keep)
{
    StringText *new_value = new(new_size) StringText;
    assert(new_value);
    if (keep)
    {
       // Copy old data into new space. Might be truncating.
       if (new_size < len) len = new_size;
       memmove(new_value->data, text, len);
    }
    else len = 0;
    FreeText();
    value = new_value;
    text = value->data;
    size = new_size;
}

// Inserts/replaces (depending on ins flag) the data pointed to 
// by s into the string. Up to n elements are inserted/replaced, 
// (truncating if necesary), starting at position p, (counted 
// by zero). The string will grow in size if inserting and needed.
// If p >= len, then the data is concatenated on the end. 
// Returns number of elements inserted/replaced, or 0 if error occurs.

unsigned String::InsReplAt(unsigned p, const char *s, unsigned n, int ins)
{
  unsigned room, needed, addval;
  if (ins)
  {
     room = size - len;
     if (n > room)
     {
        needed = (n - room);
	needed += STR_CHUNK - (needed % STR_CHUNK);
	Realloc(size + needed);
     }
  }
  EnsureUnique();

  // Don't allow gaps
  if (p >= len) p = len; // We'll be concatenating

  if (ins)
  {
     // Keep things in range. May have to truncate
     if (n > size - len) n = size - len;
     if (p < len)
     {
       // Make room for inserted data somewhere in the middle.
       memmove(text+p+n, text+p, len-p);
     }
  }
  else
  {
     // Keep things in range. May have to truncate
     if (n > size - p) n = size - p;
  }
  // Copy in source, compute final length
  memmove(text+p, (char *)s, n);
  if (ins)
     len += n;
  else
  {
     if ((p+n) > len) len = p+n;
  }
  return n;
}

// Deletes up to n elements from string at pos'n p.  If p
// is out of range nothing happens. If n == 0, nothing 
// happens. Returns number of elements deleted.

unsigned String::DeleteAt(unsigned p, unsigned n)
{
  long pel; // long is used to prevent overflows during adds
  unsigned pe;

  if (p < len && n != 0)
  {
     EnsureUnique();
     // We may be chopping off the end, so keep in range
     pel = p + n;
     if (pel > len) pel = len;
     pe = unsigned(pel); // Guaranteed to be in range
     n = pe-p;
     // Now, move elements up to take their place
     memmove(text+p, text+pe, len-pe);
     len -= n;
  }
  else n = 0;
  return n;
}

// Makes the string null-terminated by making sure a null
// byte is in the string somewhere, perhaps at the end.
// If one has to be added, the string is grown if necessary.
// If need to grow but can't, the last character is replaced,
// and the logical length of the string shrinks.
// If the string is null, or can't be made unique when a
// null byte is to be added, a "" is returned.
// Note: A null byte that's added is not accounted for in
// the logical length of the string, and thus, may be replaced
// by subsequent string operations, so use the pointer returned
// as soon as possible.
// These complicated rules ensure that the string is not
// copied or otherwise affected (as far as logical length
// is concerned) if there is room and the string is unique.

const char *String::NullTermStr()
{
  char *p = text;
  unsigned n = len;
  // Search for null byte already in string
  while(n--)
	if (*p++ == 0) return text;
  // Need to add null to end. We may need to grow.
  if (len == size) Realloc(size + STR_CHUNK);
  // Make sure we do have some room, and are unique
  assert(size>0);
  EnsureUnique();
  if (len == size) len--; // Forced to replace good character
  text[len] = 0;
  return text;
}

// Uses unsigned decimal value of characters for collating order.
// Returns -1 if a < b, 0 if a == b, and 1 if a > b.

int Compare(const String &a, const String &b)
{
  unsigned an = a.len;
  unsigned bn = b.len;
  unsigned sn = (an < bn) ? an : bn;
  unsigned char *ap = (unsigned char *)a.text;
  unsigned char *bp = (unsigned char *)b.text;

  for (unsigned i = 0; i<sn; i++)
  {
      if (*ap < *bp) return -1;
      if (*ap++ > *bp++) return 1;
  }
  // We're equal unless lengths are different
  if (an == bn) return 0;
  if (an < bn) return -1;
  return 1;
}

void String::Set(int val)
{
    char b[20];
    sprintf(b, "%d", val);
    Copy(b);
}

int String::IsNumeric()
{
    int x;
    char b[80];
    return (sscanf(text, "%d%s", &x, b)==1);
}

int String::StripList()
{
    int front = 0, back = len-1;
    while (front < len && isspace(text[front])) front++;
    if (text[front]!='{')
	return -1;
    while (back > 0 && isspace(text[back])) back--;
    if (text[back]!='}')
	return -1;
    else
    {
        String &s = String(*this, front+1, back-front-1);
    	Copy(s);
    }	
    return 0;
}

int main()
{
    String x;
    x += '{';
    x += "the ";
    x += "fat rat in the wooly hat ";
    x += "sat on the ";
    x += "cat ";
    x += '}';
    puts(x);
    x.StripList();
    puts(x);
}

