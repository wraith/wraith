/* sprintf.c
 *
 */


#include "common.h"
#include "sprintf.h"
#include "base64.h"
#include <stdarg.h>

static char *int_to_base10(int val)
{
  static char buf_base10[17] = "";

  buf_base10[16] = 0;
  if (!val) {
    buf_base10[15] = '0';
    return buf_base10 + 15;
  }

  int p = 0;
  int i = 16;

  if (val < 0) {
    p = 1;
    val *= -1;
  }
  while (val) {
    i--;
    buf_base10[i] = '0' + (val % 10);
    val /= 10;
  }
  if (p) {
    i--;
    buf_base10[i] = '-';
  }
  return buf_base10 + i;
}

static char *unsigned_int_to_base10(unsigned int val)
{
  static char buf_base10[16] = "";

  buf_base10[15] = 0;
  if (!val) {
    buf_base10[14] = '0';
    return buf_base10 + 14;
  }

  int i = 15;

  while (val) {
    i--;
    buf_base10[i] = '0' + (val % 10);
    val /= 10;
  }
  return buf_base10 + i;
}

size_t simple_vsnprintf(char *buf, size_t size, const char *format, va_list va)
{
  char *s = NULL;
  char *fp = (char *) format;
  size_t c = 0;
  unsigned int i;

  while (*fp && c < size - 1) {
    if (*fp == '%') {
      fp++;
      switch (*fp) {
      case 's':
        s = va_arg(va, char *);
        break;
      case 'd':
      case 'i':
        i = va_arg(va, int);
        s = int_to_base10(i);
        break;
      case 'D':
        i = va_arg(va, int);
        s = int_to_base64((unsigned int) i);
      case 'u':
        i = va_arg(va, unsigned int);
        s = unsigned_int_to_base10(i);
        break;
      case '%':
        buf[c++] = *fp++;
        continue;
      case 'c':
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
  buf[c] = 0;

  return c;
}

size_t simple_vsprintf(char *buf, const char *format, va_list va)
{
  return simple_vsnprintf(buf, VSPRINTF_MAXSIZE, format, va);
}

size_t simple_sprintf2 (char *buf, const char *format, ...)
{
  size_t ret = 0;

  va_list va;
  va_start(va, format);
  ret = simple_vsprintf(buf, format, va);
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

