/*
  Teem: Tools to process and visualize scientific data and images              
  Copyright (C) 2011, 2010, 2009  University of Chicago
  Copyright (C) 2008, 2007, 2006, 2005  Gordon Kindlmann
  Copyright (C) 2004, 2003, 2002, 2001, 2000, 1999, 1998  University of Utah

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public License
  (LGPL) as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  The terms of redistributing and/or modifying this software also
  include exceptions to the LGPL that facilitate static linking.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this library; if not, write to Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "air.h"

/* this has to default to false in order for airStrtok to be a
   functional substitute for strtok() */
int airStrtokQuoting = AIR_FALSE;

/*
******** airStrdup()
**
** because they didn't put strdup() in ANSI.
** This will return NULL if given NULL.
*/
char *
airStrdup(const char *s) {
  char *ret;

  if (!s) {
    ret = NULL;
  }
  else {
    ret = (char *)malloc(strlen(s)+1);
    if (ret) {
      strcpy(ret, s);
    }
  }
  return ret;
}

/*
******** airStrlen()
**
** just like strlen, but safe to call on NULL (for which return is 0)
*/
size_t
airStrlen(const char *s) {
  size_t ret;
  
  if (!s) {
    ret = 0;
  }
  else {
    ret = strlen(s);
  }
  return ret;
}

/* ---- BEGIN non-NrrdIO */
/*
******** airStrcmp
**
** just like strcmp, but safe to call with NULL strings
*/
int
airStrcmp(const char *s1, const char *s2) {
  int ret;

  if (!( s1 && s2 )) {
    ret = !!s1 - !!s2;
  } else {
    ret = strcmp(s1, s2);
  }
  return ret;
}

/* ---- END non-NrrdIO */
/*
******** airStrtok()
**
** thread-safe strtok() replacement.  Use just like strtok(), but on
** each call to parse a given string, pass as the last argument the
** address of a char*, to be used for saving state while the string is
** traversed.  Like strtok(), this will alter the "s" array passed to
** it on the first call, and like strtok(), this returns pointers into
** this string (rather than allocating new strings for each token).
*/
char *
airStrtok(char *s, const char *ct, char **last) {
  char *h, *e, *q;
  
  if (!(ct && last)) {
    /* can't do any work, bail */
    return NULL;
  }
  h = s ? s : *last;
  if (!airStrlen(h))
    return NULL;
  h += strspn(h, ct);
  if ('\"' == *h && airStrtokQuoting) {
    /* something is trying to be quoted, and, we'll respect that */
    /* have to find the next un-escaped '\"' */
    h++;
    q = h;
    while (*q && !('\"' == *q && '\\' != q[-1])) {
      q++;
    }
    if (*q) {
      /* we found an unescaped '\"' */
      e = q;
    } else {
      /* give up; pretend we never tried to do this quoting stuff */
      e = h + strcspn(h, ct);
    }
  } else {
    e = h + strcspn(h, ct);
  }
  if ('\0' == *e) {
    *last = e;
  }
  else {
    *e = '\0';
    *last = e + 1;
  }
  return h;
}

/*
******** airStrntok()
**
** returns the number of tokens parsable by airStrtok(), but does
** NOT alter the given string
*/
unsigned int
airStrntok(const char *_s, const char *ct) {
  char *s, *t, *l=NULL;
  unsigned int n = 0;

  if (_s && ct) {
    s = airStrdup(_s);
    t = airStrtok(s, ct, &l);
    while (t) {
      n++;
      t = airStrtok(NULL, ct, &l);
    }
    airFree(s);  /* no NULL assignment to s, else compile warnings */
  }
  return n;
}

char *
airStrtrans(char *s, char from, char to) {
  size_t i, l;
  
  if (s) {
    l = strlen(s);
    for (i=0; i<l; i++) {
      s[i] = (s[i] == from ? to : s[i]);
    }
  }
  return s;
}

/*
******** airStrcpy
**
** Like strncpy but logic is different (and perhaps more useful), being:
** "dst is allocated for dstSize chars. Copy as much of src as can 
** "fit in dst, and always 0-terminate the resulting dst.", 
** instead of strncpy's "Copy at most n characters, blah blah blah, 
** and you still have to 0-terminate the rest yourself".
**
** E.g. with declaration buff[AIR_STRLEN_SMALL], you call
** airStrcpy(buff, AIR_STRLEN_SMALL, src), and know that then
** strlen(buff) <= AIR_STRLEN_SMALL-1. (see note in air.h about
** the meaning of the STRLEN #defines).
**
** Returns NULL if there was a problem (NULL dst or dstSize zero),
** otherwise returns dst
*/
char *
airStrcpy(char *dst, size_t dstSize, const char *src) {
  size_t ii, copyLen, srcLen;
  
  if (!(dst && dstSize > 0)) {
    return NULL;
  }
  srcLen = airStrlen(src);
  if (1 == dstSize || !srcLen) {
    dst[0] = '\0';
    return dst;
  }
  /* else dstSize > 1  AND src is a non-empy string */
  copyLen = AIR_MIN(dstSize-1, srcLen);
  for (ii=0; ii<copyLen; ii++) {
    dst[ii] = src[ii];
  }
  dst[copyLen] = '\0';
  return dst;
}

/*
******** airEndsWith
**
** if "s" ends with "suff", then returns 1, 0 otherwise
*/
int
airEndsWith(const char *s, const char *suff) {
  
  if (!(s && suff))
    return 0;
  if (!(strlen(s) >= strlen(suff)))
    return 0;
  if (!strncmp(s + strlen(s) - strlen(suff), suff, strlen(suff)))
    return 1;
  else
    return 0;
}

/*
******** airUnescape()
**
** unescapes \\ and \n in place in a given string.
**
*/
char *
airUnescape(char *s) {
  size_t i, j, len;
  int found=0;

  len = airStrlen(s);
  if (!len) 
    return s;

  for (i=1, j=0; i<len; i++, j++) {
    if (s[i-1] == '\\' && s[i] == '\\') {
      s[j] = '\\'; i++; found = 1;
    } else if (s[i-1] == '\\' && s[i] == 'n') {
      s[j] = '\n'; i++; found = 1;
    } else {
      s[j] = s[i-1]; found = 0;
    }
  }
  if (i == len || !found) s[j++] = s[len-1];
  s[j] = 0;

  return s;
}

/*
******** airOneLinify()
**
** converts all contiguous white space (as determined by isspace()) to
** a single ' ', entirely removes non-printable (as determined by
** isprint()) characters, and entirely removes white space contiguous
** with the end of the string, even if that means shrinking the string
** to "".
**
** Useful for cleaning up lines of text to be saved as strings in
** fields of other structs
*/
char *
airOneLinify(char *s) {
  size_t i, j, len;

  len = airStrlen(s);
  if (!len) 
    return s;

  /* convert white space to space (' '), and delete unprintables */
  for (i=0; i<len; i++) {
    if (isspace(AIR_CAST(int, s[i]))) {
      s[i] = ' ';
      continue;
    }
    if (!isprint(AIR_CAST(int, s[i]))) {
      for (j=i; j<len; j++) {
        /* this will copy the '\0' at the end */
        s[j] = s[j+1];
      }
      i--;
      continue;
    }
  }

  /* compress all contiguous spaces into one */
  for (i=0; i<len; i++) {
    while (' ' == s[i] && ' ' == s[i+1]) {
      for (j=i+1; j<len; j++) {
        s[j] = s[j+1];
      }
    }
  }

  /* lose trailing white space */
  i = airStrlen(s);
  if (' ' == s[i-1]) {
    s[i-1] = '\0';
  }

  return s;
}

/*
******** airToLower()
**
** calls tolower() on all characters in a string, and returns the same
** pointer that it was given
*/
char *
airToLower(char *str) {
  char *c;

  if (str) {
    c = str;
    while (*c) {
      *c = tolower(*c);
      c++;
    }
  }
  return str;
}

/*
******** airToUpper()
**
** calls toupper() on all characters in a string, and returns the same
** pointer that it was given
*/
char *
airToUpper(char *str) {
  char *c;

  if (str) {
    c = str;
    while (*c) {
      *c = toupper(*c);
      c++;
    }
  }
  return str;
}

/*
******** airOneLine()
** 
** gets one line from "file", putting it into an array if given size.
** "size" must be the size of line buffer "line": the size which
** "line" was allocated for, not the number of non-null characters it
** was meant to hold.  "size" must be at least 3.  Always
** null-terminates the contents of the array (except if the arguments
** are invalid).  The idea is that the null-termination replaces the
** line termination.
**
** 0: if saw EOF before seeing a newline, or arguments are invalid
** 1: if line was a single newline
** n; n <= size: if line was n-1 characters followed by newline
** size+1: if didn't see a newline within size-1 characters
**
** So except for returns of -1 and size+1, the return is the number of
** characters comprising the line, including the newline character.
**
** For all you DOS\Windows\Cygwin users, this will quietly pretend that
** a "\r\n" pair is really just "\n", including the way that characters
** comprising the line are counted.  However, there is no pretension
** that on those platforms, "\n" by itself does not actually count as
** a newline.
**
** Finally, for those trafficking in legacy Mac text files (for which
** the line termination is only "\r", the same applies- these are also
** effectively treated the same as a newline.
*/
unsigned int
airOneLine(FILE *file, char *line, int size) {
  int c=0, i;
  
  if (!(size >= 3  /* need room for a character and a Windows newline */
        && line && file)) {
    return 0;
  }
  /* c is always set at least once, but not so for any char in line[]  */
  for (i=0;
       (i <= size-2              /* room for line[i] and \0 after that */
        && EOF != (c=getc(file)) /* didn't hit EOF trying to read char */
        && c != '\n'             /* char isn't newline */
	&& c != '\r');           /* char isn't carriage return */
       ++i) {
    line[i] = c;
  }

  if (EOF == c) {
    /* for-loop terminated because we hit EOF */
    line[0] = '\0';
    return 0;
  } else if ('\r' == c || '\n' == c) {
    /* for-loop terminated because we hit '\n' or '\r' */
    /* if it was '\r', see if next character is '\n' */
    if ('\r' == c) {
      c = getc(file);
      if (EOF != c && '\n' != c) {
	/* oops, we got something, and it was not a '\n'; put it back */
        ungetc(c, file);
      }
    }
    line[i] = '\0';
    return i+1;
  } else {
    /* for-loop terminated because we got to end of buffer (i == size-1) */
    c = getc(file);
    /* but see if we were about to get '\r', "\r\n", or '\n' */
    if ('\r' == c) {
      int d;
      d = getc(file);
      if (EOF != d && '\n' != d) {
	/* oops, put it back */
        ungetc(d, file);
      }
      line[i] = '\0';
      return i+1;
    } else if ('\n' == c) {
      line[i] = '\0';
      return i+1;
    } else {
      /* weren't about to get a line termination,
	 we really did run out of buffer */
      if (EOF != c) {
        ungetc(c, file);  /* we're allowed one ungetc on ANY stream */
      }
      line[size-1] = '\0';
      return size+1;
    }
  }
}

