#ifndef _SPRINTFH_H
#define _SPRINTF_H


#include <sys/types.h>

size_t simple_sprintf (char *, const char *, ...);
char *int_to_base10(int);
char *unsigned_int_to_base10(unsigned int);
char *int_to_base64(unsigned int);

#endif /* _SPRINTF_H */
