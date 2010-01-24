/*
 * memset.c -- provides memset() if necessary.
 *
 */

#include "memset.h"

#ifndef HAVE_MEMSET
void *memset(void *dest, int c, size_t n)
{
  while (n--)
    *((unsigned char *) dest)++ = c;
  return dest;
}
#endif /* !HAVE_MEMSET */
