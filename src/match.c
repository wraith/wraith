#include "main.h"
#define QUOTE '\\'
#define WILDS '*'
#define WILDP '%'
#define WILDQ '?'
#define WILDT '~'
#define NOMATCH 0
#define MATCH (match+sofar)
#define PERMATCH (match+saved+sofar)
int
_wild_match_per (register unsigned char *m, register unsigned char *n)
{
  unsigned char *ma = m, *lsm = 0, *lsn = 0, *lpm = 0, *lpn = 0;
  int match = 1, saved = 0, space;
  register unsigned int sofar = 0;
  if ((m == 0) || (n == 0) || (!*n))
    return NOMATCH;
  while (*n)
    {
      if (*m == WILDT)
	{
	  space = 0;
	  do
	    {
	      m++;
	      space++;
	    }
	  while ((*m == WILDT) || (*m == ' '));
	  sofar += space;
	  while (*n == ' ')
	    {
	      n++;
	      space--;
	    }
	  if (space <= 0)
	    continue;
	}
      else
	{
	  switch (*m)
	    {
	    case 0:
	      do
		m--;
	      while ((m > ma) && (*m == '?'));
	      if ((m > ma) ? ((*m == '*') && (m[-1] != QUOTE)) : (*m == '*'))
		return PERMATCH;
	      break;
	    case WILDP:
	      while (*(++m) == WILDP);
	      if (*m != WILDS)
		{
		  if (*n != ' ')
		    {
		      lpm = m;
		      lpn = n;
		      saved += sofar;
		      sofar = 0;
		    }
		  continue;
		}
	    case WILDS:
	      do
		m++;
	      while ((*m == WILDS) || (*m == WILDP));
	      lsm = m;
	      lsn = n;
	      lpm = 0;
	      match += (saved + sofar);
	      saved = sofar = 0;
	      continue;
	    case WILDQ:
	      m++;
	      n++;
	      continue;
	    case QUOTE:
	      m++;
	    }
	  if (rfc_toupper (*m) == rfc_toupper (*n))
	    {
	      m++;
	      n++;
	      sofar++;
	      continue;
	    }
#ifdef WILDT
	}
#endif
      if (lpm)
	{
	  n = ++lpn;
	  m = lpm;
	  sofar = 0;
	  if ((*n | 32) == 32)
	    lpm = 0;
	  continue;
	}
      if (lsm)
	{
	  n = ++lsn;
	  m = lsm;
	  saved = sofar = 0;
	  continue;
	}
      return NOMATCH;
    }
  while ((*m == WILDS) || (*m == WILDP))
    m++;
  return (*m) ? NOMATCH : PERMATCH;
}

int
_wild_match (register unsigned char *m, register unsigned char *n)
{
  unsigned char *ma = m, *na = n, *lsm = 0, *lsn = 0;
  int match = 1;
  register int sofar = 0;
  if ((ma == 0) || (na == 0) || (!*ma) || (!*na))
    return NOMATCH;
  while (*(++m));
  m--;
  while (*(++n));
  n--;
  while (n >= na)
    {
      switch (*m)
	{
	case WILDS:
	  do
	    m--;
	  while ((m >= ma) && (*m == WILDS));
	  lsm = m;
	  lsn = n;
	  match += sofar;
	  sofar = 0;
	  continue;
	case WILDQ:
	  m--;
	  n--;
	  continue;
	}
      if (rfc_toupper (*m) == rfc_toupper (*n))
	{
	  m--;
	  n--;
	  sofar++;
	  continue;
	}
      if (lsm)
	{
	  n = --lsn;
	  m = lsm;
	  if (n < na)
	    lsm = 0;
	  sofar = 0;
	  continue;
	}
      return NOMATCH;
    }
  while ((m >= ma) && (*m == WILDS))
    m--;
  return (m >= ma) ? NOMATCH : MATCH;
}
