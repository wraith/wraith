#ifndef _BINARY_H
#  define _BINARY_H

#include "conf.h"

extern int checked_bin_buf;

#  define GET_CHECKSUM		BIT0
#  define WRITE_CHECKSUM 	BIT1
#  define WRITE_PACK            BIT2
#  define WRITE_CONF            BIT3

void check_sum(const char *, const char *);
void write_settings(const char *, int, int);
void conf_to_bin(conf_t *, bool, int);
#endif /* !_BINARY_H */
