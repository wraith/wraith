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
 * rfc1459.c
 * 
 */


#include "common.h"
#include "rfc1459.h"

int (*rfc_casecmp) (const char *, const char *) = _rfc_casecmp;
int (*rfc_ncasecmp) (const char *, const char *, size_t) = _rfc_ncasecmp;
bool (*rfc_char_equal) (const char, const char)  = _rfc_char_equal;

int
_rfc_casecmp(const char *s1, const char *s2)
{
  while ((*s1) && (*s2) && _rfc_char_equal(*s1, *s2)) {
    ++s1;
    ++s2;
  }
  return _rfc_toupper(*s1) - _rfc_toupper(*s2);
}

int
_rfc_ncasecmp(const char *s1, const char *s2, size_t n)
{
  if (!n)
    return 0;
  while (--n && (*s1) && (*s2) && _rfc_char_equal(*s1, *s2)) {
    ++s1;
    ++s2;
  }
  return _rfc_toupper(*s1) - _rfc_toupper(*s2);
}

/* vim: set sts=2 sw=2 ts=8 et: */
