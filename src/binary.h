#ifndef _BINARY_H
#  define _BINARY_H

#include "conf.h"

extern int checked_bin_buf;

#  define WRITE_CHECKSUM 	1
#  define GET_CHECKSUM		2

void check_sum(const char *, const char *);
void write_settings(const char *, int);
void conf_to_bin(conf_t *, bool);
#endif /* !_BINARY_H */
