/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * match.c
 *   wildcard matching functions
 *
 *
 * Once this code was working, I added support for % so that I could
 * use the same code both in Eggdrop and in my IrcII client.
 * Pleased with this, I added the option of a fourth wildcard, ~,
 * which matches varying amounts of whitespace (at LEAST one space,
 * though, for sanity reasons).
 * 
 * This code would not have been possible without the prior work and
 * suggestions of various sourced.  Special thanks to Robey for
 * all his time/help tracking down bugs and his ever-helpful advice.
 * 
 * 04/09:  Fixed the "*\*" against "*a" bug (caused an endless loop)
 * 
 *   Chris Fuller  (aka Fred1@IRC & Fwitz@IRC)
 *     crf@cfox.bchs.uh.edu
 * 
 * I hereby release this code into the public domain
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "match.h"
#include "misc.h"
#include "rfc1459.h"
#include "socket.h"

#define QUOTE '\\' /* quoting character (overrides wildcards) */
#define WILDS '*'  /* matches 0 or more characters (including spaces) */
#define WILDP '%'  /* matches 0 or more non-space characters */
#define WILDQ '?'  /* matches ecactly one character */
#define WILDT '~'  /* matches 1 or more spaces */

#define NOMATCH 0
#define MATCH (match+sofar)
#define PERMATCH (match+saved+sofar)

/* binds matching */
int _wild_match_per(unsigned char *m, unsigned char *n)
{
  /* null strings should never match */
  if ((m == 0) || (n == 0) || (!*n))
    return NOMATCH;

  unsigned char *ma = m, *lsm = NULL, *lsn = NULL, *lpm = NULL, *lpn = NULL;
  int match = 1, saved = 0, space;
  int sofar = 0;

  while (*n) {
    if (*m == WILDT) {          /* Match >=1 space */
      space = 0;                /* Don't need any spaces */
      do {
        m++;
        space++;
      }                         /* Tally 1 more space ... */
      while ((*m == WILDT) || (*m == ' '));     /*  for each space or ~ */
      sofar += space;           /* Each counts as exact */
      while (*n == ' ') {
        n++;
        space--;
      }                         /* Do we have enough? */
      if (space <= 0)
        continue;               /* Had enough spaces! */
    }
    /* Do the fallback       */
    else {
      switch (*m) {
      case 0:
        do
          m--;                  /* Search backwards */
        while ((m > ma) && (*m == '?'));        /* For first non-? char */
        if ((m > ma) ? ((*m == '*') && (m[-1] != QUOTE)) : (*m == '*'))
          return PERMATCH;      /* nonquoted * = match */
        break;
      case WILDP:
        while (*(++m) == WILDP)
          ;        /* Zap redundant %s */
        if (*m != WILDS) {      /* Don't both if next=* */
          if (*n != ' ') {      /* WILDS can't match ' ' */
            lpm = m;
            lpn = n;            /* Save '%' fallback spot */
            saved += sofar;
            sofar = 0;          /* And save tally count */
          }
          continue;             /* Done with '%' */
        }
        /* FALL THROUGH */
      case WILDS:
        do
          m++;                  /* Zap redundant wilds */
        while ((*m == WILDS) || (*m == WILDP));
        lsm = m;
        lsn = n;
        lpm = 0;                /* Save '*' fallback spot */
        match += (saved + sofar);       /* Save tally count */
        saved = sofar = 0;
        continue;               /* Done with '*' */
      case WILDQ:
        m++;
        n++;
        continue;               /* Match one char */
      case QUOTE:
        m++;                    /* Handle quoting */
      }
      if (rfc_char_equal(*m, *n)) { /* If matching */
        m++;
        n++;
        sofar++;
        continue;               /* Tally the match */
      }
#ifdef WILDT
    }
#endif
    if (lpm) {                  /* Try to fallback on '%' */
      n = ++lpn;
      m = lpm;
      sofar = 0;                /* Restore position */
      if ((*n | 32) == 32)
        lpm = 0;                /* Can't match 0 or ' ' */
      continue;                 /* Next char, please */
    }
    if (lsm) {                  /* Try to fallback on '*' */
      n = ++lsn;
      m = lsm;                  /* Restore position */
      saved = sofar = 0;
      continue;                 /* Next char, please */
    }
    return NOMATCH;             /* No fallbacks=No match */
  }
  while ((*m == WILDS) || (*m == WILDP))
    m++;                        /* Zap leftover %s & *s */
  return (*m) ? NOMATCH : PERMATCH;     /* End of both = match */
}


/* general/host matching */
int _wild_match(unsigned char *m, unsigned char *n)
{
  unsigned char *ma = m, *na = n;

  /* null strings should never match */
  if ((ma == 0) || (na == 0) || (!*ma) || (!*na))
    return NOMATCH;

  unsigned char *lsm = NULL, *lsn = NULL;
  int match = 1;
  int sofar = 0;

  /* find the end of each string */
  while (*(++m))
    ;
  m--;
  while (*(++n))
    ;
  n--;

  while (n >= na) {
    /* If the mask runs out of chars before the string, fall back on
     * a wildcard or fail. */
    if (m < ma) {
      if (lsm) {
        n = --lsn;
        m = lsm;
        if (n < na) lsm = 0;
        sofar = 0;
      }
      else return NOMATCH;
    }
    switch (*m) {
    case WILDS:                /* Matches anything */
      do
        m--;                    /* Zap redundant wilds */
      while ((m >= ma) && (*m == WILDS));
      lsm = m;
      lsn = n;
      match += sofar;
      sofar = 0;                /* Update fallback pos */
      if (m < ma) return MATCH;
      continue;                 /* Next char, please */
    case WILDQ:
      m--;
      n--;
      continue;                 /* '?' always matches */
    }
    if (rfc_char_equal(*m, *n)) {   /* If matching char */
      m--;
      n--;
      sofar++;                  /* Tally the match */
      continue;                 /* Next char, please */
    }
    if (lsm) {                  /* To to fallback on '*' */
      n = --lsn;
      m = lsm;
      if (n < na)
        lsm = 0;                /* Rewind to saved pos */
      sofar = 0;
      continue;                 /* Next char, please */
    }
    return NOMATCH;             /* No fallback=No match */
  }
  while ((m >= ma) && (*m == WILDS))
    m--;                        /* Zap leftover %s & *s */
  return (m >= ma) ? NOMATCH : MATCH;   /* Start of both = match */
}

static inline int
comp_with_mask(void *addr, void *dest, unsigned int mask)
{
  int n = mask >> 3;

  if (memcmp(addr, dest, n) == 0)
  {
    int leftover = mask % 8;

    if (leftover == 0)
      return (1);

    int m = ((~0U) << (8 - leftover));

    if ((((unsigned char *) addr)[n] & m) == (((unsigned char *) dest)[n] & m))
      return (1);
  }
  return (0);
}

/* match_cidr()
 *
 * Input - mask, address
 * Ouput - + = Matched 0 = Did not match
 */

int
match_cidr(const char *m, const char *a)
{
  const char *len_p = strrchr(m, '/');
  if(len_p == NULL)
    return 0;

  const char *ip_mask_p = strrchr(m, '@');
  if(ip_mask_p == NULL)
    return 0;

  const char *ip_p = strrchr(a, '@');
  if(ip_p == NULL)
    return 0;

  char mask[NICKLEN + UHOSTLEN + 6] = "", *ipmask = NULL, *ip = NULL, *len = NULL;

  strlcpy(mask, m, sizeof(mask));
  ipmask = mask + (ip_mask_p - m);

  char address[NICKLEN + UHOSTLEN + 6] = "";

  strlcpy(address, a, sizeof(address));
  ip = address + (ip_p - a);

  *ipmask++ = '\0';

  len = mask + (len_p - m);
  *len++ = '\0';

  if (!str_isdigit(len))
    return 0;

  int cidrlen = atoi(len);
  if(cidrlen <= 0)
    return 0;

  *ip++ = '\0';

  int ret = 0;
  int aftype = 0;

  sockname_t ipaddr, maskaddr;
  bzero(&ipaddr, sizeof(ipaddr));
  bzero(&maskaddr, sizeof(maskaddr));

  if (!strchr(ip, ':') && !strchr(ipmask, ':'))
    aftype = ipaddr.family = maskaddr.family =  AF_INET;
#ifdef USE_IPV6
  else if (strchr(ip, ':') && strchr(ipmask, ':'))
    aftype = ipaddr.family = maskaddr.family = AF_INET6;
#endif /* USE_IPV6 */
  else
    return 0;

#ifdef USE_IPV6
  if (aftype == AF_INET6) {
    if (cidrlen > 128) 
      return 0;
    inet_pton(aftype, ip, &ipaddr.u.ipv6.sin6_addr);
    inet_pton(aftype, ipmask, &maskaddr.u.ipv6.sin6_addr);
    if (comp_with_mask(&ipaddr.u.ipv6.sin6_addr.s6_addr, &maskaddr.u.ipv6.sin6_addr.s6_addr, cidrlen) && 
       ((ret = wild_match(mask, address))))
      return ret;
  } else if (aftype == AF_INET) {
#endif /* USE_IPV6 */
    if (cidrlen > 32) 
      return 0;
    inet_pton(aftype, ip, &ipaddr.u.ipv4.sin_addr);
    inet_pton(aftype, ipmask, &maskaddr.u.ipv4.sin_addr);
    if (comp_with_mask(&ipaddr.u.ipv4.sin_addr.s_addr, &maskaddr.u.ipv4.sin_addr.s_addr, cidrlen) && 
       ((ret = wild_match(mask, address))))
      return ret;
#ifdef USE_IPV6
  }
#endif
  return 0;
}

/* vim: set sts=2 sw=2 ts=8 et: */
