#ifndef _CHANPROG_H
#define _CHANPROG_H

#include "src/chan.h"

#define DO_LOCAL	1
#define DO_NET		2
#define CMD		4

int do_chanset(char *, struct chanset_t *, const char *, int);
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
int botshouldjoin(struct userrec *u, struct chanset_t *);
int shouldjoin(struct chanset_t *);
char *samechans(const char *, const char *);
void add_myself_to_userlist();
void load_internal_users();

extern struct chanset_t		*chanset;
extern char			admin[], origbotname[NICKLEN + 1], botname[];
extern port_t			my_port;
extern bool			reset_chans;

#endif /* !_CHANPROG_H */
