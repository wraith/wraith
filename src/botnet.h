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
char *lastbot(char *);
int nextbot(char *);
int in_chain(char *);
#ifdef HUB
void tell_bots(int, int, char *);
void tell_bottree(int);
#endif /* HUB */
void dump_links(int);
int botlink(char *, int, char *);
int botunlink(int, char *, char *);
void addbot(char *, char *, char *, char, int, time_t, char *);
void updatebot(int, char *, char, int, time_t, char *);
void rembot(const char *);
struct tand_t_struct *findbot(char *);
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
int getparty(char *, int);

#endif /* !_BOTNET_H */
