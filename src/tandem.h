/*
 * tandem.h
 *
 */

#ifndef _EGG_TANDEM_H
#define _EGG_TANDEM_H

/* Keep track of tandem-bots in the chain */
typedef struct tand_t_struct {
  struct tand_t_struct *via;
  struct tand_t_struct *uplink;
  struct tand_t_struct *next;
  time_t buildts;
  int localhub;
  int fflags;
  struct userrec* u;
  char commit[10];
  char bot[HANDLEN + 1];
  char version[151];
  char share;
  bool hub;
} tand_t;

/* Keep track of party-line members */
typedef struct {
  time_t timer;			/* Track idle time */
  size_t status;
  int sock;
  int chan;
  char *from;
  char *away;
  char nick[HANDLEN + 1];
  char bot[HANDLEN + 1];
  char flag;
} party_t;

/* Status: */
#define PLSTAT_AWAY   0x001
#define IS_PARTY      0x002

/* Minimum version that uses tokens & base64 ints
 * for channel msg's
 */
#define NEAT_BOTNET 1000000
#define GLOBAL_CHANS 100000


void botnet_send_chan(int, const char *, const char *, int, const char *);
void botnet_send_chat(int, const char *, const char *);
void botnet_send_act(int, const char *, const char *, int, const char *);
void botnet_send_ping(int);
void botnet_send_pong(int);
void botnet_send_priv (int, const char *, const char *, const char *, const char *, ...) __attribute__((format(printf, 5, 6)));
void botnet_send_who(int, const char *, const char *, int);
void botnet_send_unlinked(int, const char *, const char *);
void botnet_send_traced(int, const char *, const char *);
void botnet_send_trace(int, const char *, const char *, const char *);
void botnet_send_unlink(int, const char *, const char *, const char *, const char *);
void botnet_send_link(int, const char *, const char *, const char *);
void botnet_send_update(int, tand_t *);
void botnet_send_nlinked(int, const char *, const char *, const char, int, time_t, const char *, const char *, int);
void botnet_send_reject(int, const char *, const char *, const char *, const char *, const char *);
void botnet_send_log(int, const char *, int, const char *, bool = 0);
void botnet_send_zapf(int, const char *, const char *, const char *);
void botnet_send_zapf_broad(int, const char *, const char *, const char *);
void botnet_send_away(int, const char *, int, const char *, int);
void botnet_send_idle(int, const char *, int, int, const char *);
void botnet_send_join_idx(int);
void botnet_send_join_party(int, int, int);
void botnet_send_part_idx(int, const char *);
void botnet_send_part_party(int, int, const char *, int);
void botnet_send_bye(const char *);
void botnet_send_nkch_part(int, int, const char *);
void botnet_send_nkch(int, const char *);
int bots_in_subtree(const tand_t *) __attribute__((pure));
int users_in_subtree(const tand_t *) __attribute__((pure));
int botnet_send_cmd(const char * fbot, const char * bot, const char *fhnd, int fromidx, const char * cmd);
void botnet_send_cmd_broad(int idx, const char * fbot, const char *fhnd, int fromidx, const char * cmd);
void botnet_send_cmdreply(const char * fbot, const char * bot, const char * to, const char * toidx, const char * ln);
void send_uplink(const char *, size_t);
#define send_hubs(_s, _l) send_hubs_but(-1, (_s), (_l))
void send_hubs_but(int , const char *, size_t);


#define b_status(a)	(dcc[a].status)

#endif				/* _EGG_TANDEM_H */
