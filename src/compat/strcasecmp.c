/*
 * strcasecmp.c -- provides strcasecmp() and strncasecmp if necessary.
 *
 */


#include "memcpy.h"

#ifndef HAVE_STRCASECMP
int strcasecmp(const char *s1, const char *s2)
{
  while ((*s1) && (*s2) && (toupper(*s1) == toupper(*s2))) {
    s1++;
    s2++;
  }
  return toupper(*s1) - toupper(*s2);
}
#endif /* !HAVE_STRCASECMP */

#ifndef HAVE_STRNCASECMP
int strncasecmp(const char *s1, const char *s2, size_t n)
{
  if (!n)
    return 0;
  while (--n && (*s1) && (*s2) && (toupper(*s1) == toupper(*s2))) {
    s1++;
    s2++;
  }
  return toupper(*s1) - toupper(*s2);
}
#endif /* !HAVE_STRNCASECMP */
