#ifndef _BOTNET_H
#define _BOTNET_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "tandem.h"

extern tand_t		*tandbot;
extern party_t		*party;
extern int		tands;

void lower_bot_linked(int idx);
void answer_local_whom(int, int);
char *lastbot(const char *) __attribute__((pure));
int nextbot(const char *) __attribute__((pure));
int in_chain(const char *) __attribute__((pure));
void tell_bots(int, int, const char *);
void tell_bottree(int);
void dump_links(int);
int botlink(char *, int, char *);
int botunlink(int, const char *, const char *);
void addbot(char *, char *, char *, char, int, time_t, char *, char *, int);
void updatebot(int, char *, char, int, time_t, char *, char *, int);
void rembot(const char *);
tand_t *findbot(const char *) __attribute__((pure));
void unvia(int, struct tand_t_struct *);
void check_botnet_pings();
int addparty(char *, char *, int, char, int, char *, int *);
void remparty(char *, int);
void partystat(char *, int, int, int);
int partynick(char *, int, char *);
bool partyidle(char *, char *);
void partysetidle(char *, int, int);
void partyaway(char *, int, char *);
void botnet_send_cmdpass(int, char *, char *);
void zapfbot(int);
void tandem_relay(int, char *, int);
int getparty(const char *, int) __attribute__((pure));
void init_party(void);
#endif /* !_BOTNET_H */
