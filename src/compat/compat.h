/*
 * compat.h
 *   wrap-around header for all compability functions.
 *
 */

#ifndef _EGG_COMPAT_COMPAT_H
#define _EGG_COMPAT_COMPAT_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "dirname.h"
#include "dn_expand.h"
#include "snprintf.h"
#include "memutil.h"
#include "strlcpy.h"
#include "strsep.h"
#include "timespec.h"

#ifdef __cplusplus
extern "C" {
#endif
#ifndef HAVE_MEMMEM
void *memmem(const void *l, size_t l_len, const void *s, size_t s_len);
#endif
#ifdef __cplusplus
}
#endif

/* These apparently are unsafe without recasting. */
#define egg_isdigit(x)  isdigit((int)  (unsigned char) (x))
#define egg_isxdigit(x) isxdigit((int) (unsigned char) (x))
#define egg_isspace(x)  isspace((int)  (unsigned char) (x))
#define egg_islower(x)  islower((int)  (unsigned char) (x))
#define egg_isupper(x)  isupper((int)  (unsigned char) (x))
#define egg_isprint(x)  isprint((int)  (unsigned char) (x))


#endif /* !__EGG_COMPAT_COMPAT_H */

