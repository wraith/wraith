/*
 * memcpy.c -- provides memcpy() if necessary.
 *
 */

#include "main.h"
#include "memcpy.h"

#ifndef HAVE_MEMCPY
void *
egg_memcpy (void *dest, const void *src, size_t n)
{
  while (n--)
    *((char *) dest)++ = *((char *) src)++;
  return dest;
}
#endif /* !HAVE_MEMCPY */
