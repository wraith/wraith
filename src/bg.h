#ifndef _BG_H
#define _BG_H

#include <sys/types.h>

extern time_t 		lastfork;
extern pid_t 		watcher;

pid_t do_fork();
int close_tty();
void writepid(const char *, pid_t);

#endif /* !_BG_H */
