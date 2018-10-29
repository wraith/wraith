#ifndef _SPRINTF_H
#define _SPRINTF_H


#include <sys/types.h>
#include <stdarg.h>

#define VSPRINTF_MAXSIZE		4096

size_t simple_vsnprintf(char *, size_t, const char *, va_list) __attribute__((format(printf, 3, 0)));
size_t simple_vsprintf(char *, const char *, va_list) __attribute__((format(printf, 2, 0)));
size_t simple_sprintf (char *, const char *, ...) __attribute__((format(printf, 2, 3)));
size_t simple_snprintf2 (char *, size_t, const char *, ...);
size_t simple_snprintf (char *, size_t, const char *, ...) __attribute__((format(printf, 3, 4)));

#endif /* _SPRINTF_H */
