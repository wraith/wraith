#ifndef _CHANPROG_H
#define _CHANPROG_H

#include "src/chan.h"

#define DO_LOCAL	1
#define DO_NET		2

extern struct chanset_t *chanset;
extern char botname[];

int do_chanset(char *, struct chanset_t *, char *, int);
void checkchans(int);
void tell_verbose_uptime(int);
void tell_verbose_status(int);
void tell_settings(int);
int isowner(char *);
void reaffirm_owners();
void reload();
void chanprog();
void check_timers();
void check_utimers();
void rmspace(char *s);
void set_chanlist(const char *host, struct userrec *rec);
void clear_chanlist(void);
void clear_chanlist_member(const char *nick);
int shouldjoin(struct chanset_t *);


extern struct chanset_t		*chanset;
extern char			admin[], origbotname[], botname[];
#ifdef HUB
extern int			my_port;
#endif /* HUB */

#endif /* !_CHANPROG_H */
