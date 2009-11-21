#ifndef _BG_H
#define _BG_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

extern time_t 		lastfork;

pid_t do_fork();
int close_tty();
void writepid(const char *, pid_t);

#endif /* !_BG_H */
