/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2008 Bryan Drewery
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

/* sprintf.c
 *
 */


#include "common.h"
#include "sprintf.h"
#include "base64.h"
#include <stdarg.h>

static char buf_convert[20] = "";
static char *int_to_string(const long val, const unsigned int base)
{
  buf_convert[sizeof(buf_convert) - 1] = 0;
  if (!val) {
    buf_convert[sizeof(buf_convert) - 2] = '0';
    return buf_convert + sizeof(buf_convert) - 2;
  }

  bool negative = 0;
  long uval = val;

  if (val < 0) {
    negative = 1;
    uval = -val;
  }

  int place = sizeof(buf_convert) - 1;
  while (uval && place >= 0) {
    buf_convert[--place] = '0' + uval % base;
    uval /= base;
  }

  if (negative)
    buf_convert[--place] = '-';
 
  return buf_convert + place;
}

static char *unsigned_int_to_string(unsigned long val, const unsigned int base, bool caps = false)
{
  buf_convert[sizeof(buf_convert) - 1] = 0;
  if (!val) {
    buf_convert[sizeof(buf_convert) - 2] = '0';
    return buf_convert + sizeof(buf_convert) - 2;
  }

  int place = sizeof(buf_convert) - 1;

  while (val && place >= 0) {
    buf_convert[--place] = (caps ? "0123456789ABCDEF" : "0123456789abcdef")[val % base];
    val /= base;
  }
  return buf_convert + place;
}

size_t simple_vsnprintf(char *buf, size_t size, const char *format, va_list va)
{
  char *s = NULL;
  char *fp = (char *) format;
  size_t c = 0;
  unsigned long i = 0;
  bool islong = 0, caps = false;

  while (*fp && c < size - 1) {
    if (*fp == '%') {
re_eval_with_modifier:
      ++fp;
      switch (*fp) {
      case 'l':
        islong = 1;
        goto re_eval_with_modifier;
      case 's': //string
        s = va_arg(va, char *);
        break;
      case 'd': //int
      case 'i':
        if (islong) {
          i = va_arg(va, long);
          islong = 0;
        } else
          i = va_arg(va, int);
        s = int_to_string(i, 10);
        break;
      case 'D': //stupid eggdrop base64
        i = va_arg(va, int);
        s = int_to_base64((unsigned int) i);
        break;
      case 'u': //unsigned
        if (islong) {
          i = va_arg(va, unsigned long);
          islong = 0;
        } else
          i = va_arg(va, unsigned int);
        s = unsigned_int_to_string(i, 10);
        break;
      case 'X': //HEX
        caps = true;
      case 'x': //hex
        if (islong) {
          i = va_arg(va, unsigned long);
          islong = 0;
        } else
          i = va_arg(va, unsigned int);
        s = unsigned_int_to_string(i, 16, caps);
        caps = false;
        break;
      case '%': //%
        buf[c++] = *fp++;
        continue;
      case 'c': //char
        buf[c++] = (char) va_arg(va, int);
        fp++;
        continue;
      default:
        continue;
      }
      if (s)
        while (*s && c < size - 1)
          buf[c++] = *s++;
      fp++;
    } else
      buf[c++] = *fp++;
  }
  buf[c] = '\0';

  return c;
}

size_t simple_vsprintf(char *buf, const char *format, va_list va)
{
  return simple_vsnprintf(buf, VSPRINTF_MAXSIZE, format, va);
}

size_t simple_snprintf2 (char *buf, size_t size, const char *format, ...)
{
  size_t ret = 0;

  va_list va;
  va_start(va, format);
  ret = simple_vsnprintf(buf, size, format, va);
  va_end(va);
  return ret;
}

size_t simple_sprintf (char *buf, const char *format, ...)
{
  size_t ret = 0;

  va_list va;
  va_start(va, format);
  ret = simple_vsprintf(buf, format, va);
  va_end(va);
  return ret;
}

size_t simple_snprintf (char *buf, size_t size, const char *format, ...)
{
  size_t ret = 0;

  va_list va;
  va_start(va, format);
  ret = simple_vsnprintf(buf, size, format, va);
  va_end(va);
  return ret;
}

