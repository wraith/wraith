#ifndef _BG_H
#define _BG_H

#ifndef MAKING_MODS
extern time_t 		lastfork;

void do_fork();
void bg_do_split();
#endif /* !MAKING_MODS */

#endif /* !_BG_H */
