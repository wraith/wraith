#ifndef _BG_H
#define _BG_H

#include <sys/types.h>

extern time_t 		lastfork;
extern pid_t 		watcher;

void do_fork();
void bg_do_split();

#endif /* !_BG_H */
