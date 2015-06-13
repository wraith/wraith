#ifndef _DIRNAME_H
#define _DIRNAME_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define dirname my_dirname

#ifdef __cplusplus
extern "C" {
#endif
char *dirname(const char *);
#ifdef __cplusplus
}
#endif

#endif /* !_DIRNAME_H */
