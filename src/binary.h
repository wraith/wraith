#ifndef _BINARY_H
#  define _BINARY_H

#include "conf.h"

extern int checked_bin_buf;

#  define WRITE_MD5 	1
#  define GET_MD5		2

void check_sum(const char *, const char *);
void write_settings(const char *);
void conf_to_bin(conf_t *);
#endif /* !_BINARY_H */
