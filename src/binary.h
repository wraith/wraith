#ifndef _BINARY_H
#  define _BINARY_H

#include "conf.h"

extern int checked_bin_buf;

#  define GET_CHECKSUM		BIT0
#  define WRITE_CHECKSUM 	BIT1
#  define WRITE_PACK            BIT2
#  define WRITE_CONF            BIT3
#  define GET_CONF              BIT4

void check_sum(const char *, const char *, bool);
void write_settings(const char *, int, bool, int initialized = -1);
int check_bin_initialized(const char *fname);
bool check_bin_compat(const char *fname);
void conf_to_bin(conf_t *, bool, int);
void reload_bin_data();
#endif /* !_BINARY_H */
