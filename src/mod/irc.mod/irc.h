/*
 * irc.h -- part of irc.mod
 *
 */

#ifndef _EGG_MOD_IRC_IRC_H
#define _EGG_MOD_IRC_IRC_H

#include "src/auth.h"

enum { BC_NOCOOKIE = 1, BC_SLACK, BC_HASH, BC_COUNTER };

//#define REVENGE_KICK 1		/* Kicked victim	*/
//#define REVENGE_DEOP 2		/* Took op		*/

#define PRIO_DEOP 1
#define PRIO_KICK 2

/* For flushmodes */
#define NORMAL          0
#define QUICK           1

#ifdef MAKING_IRC

namespace bd {
  class String;
}

#ifdef CACHE
typedef struct cache_chan_b {
  struct cache_chan_b *next;
  bool invite;		/* set to invite on userhost */
  bool ban;		/* set to ban on join */
  bool op;		/* should we auto-op? */
  bool invited;		/* set when we've called cache_invite, meaning we aren't hijacked */
  char dname[81];
} cache_chan_t;

typedef struct cache_b {
  struct cache_b *next;
  cache_chan_t *cchan;
//  struct chanset_t *chan;
//  struct userrec *user;
  time_t timeval;
//  bool invite;			/* INVITE ON USERHOST */
//  bool ban;			/* BAN ON USERHOST */
//  bool invited;			/* INVITED - CLEARED */
  bool bot;
  char nick[NICKLEN];
  char handle[NICKLEN];
  char uhost[UHOSTLEN];
} cache_t;

static void cache_chan_del(char *, char *);
//static cache_chan_t *cache_chan_find(cache_t *, char *, char *);
static void cache_chan_find(cache_t *, cache_chan_t *, char *, char *);
static cache_chan_t *cache_chan_add(cache_t *, char *);
static cache_t *cache_find(char *);
static cache_t *cache_new(char *);
static void cache_del(char *, cache_t *);
static void cache_debug(void);
#endif /* CACHE */
static void cache_invite(struct chanset_t *, char *, char *, char *, bool, bool);

//static char *makecookie(const char *, const memberlist*, const memberlist*, const memberlist* = NULL, const memberlist* = NULL);
void makecookie(char*, size_t, const char *, const memberlist*, const memberlist*, const memberlist* = NULL, const memberlist* = NULL);
static int checkcookie(const char*, const memberlist*, const memberlist*, const char*, int);
extern void counter_clear(const char*);
static bool any_ops(struct chanset_t *);
static char *getchanmode(struct chanset_t *);
static void flush_mode(struct chanset_t *, int);
static bool member_getuser(memberlist* m, bool act_on_lookup = 0);
static void do_protect(struct chanset_t* chan, const char* reason);

/* reset(bans|exempts|invites) are now just macros that call resetmasks
 * in order to reduce the code duplication. <cybah>
 */
#define resetbans(chan)	    resetmasks((chan), (chan)->channel.ban, (chan)->bans, global_bans, 'b')
#define resetexempts(chan)  resetmasks((chan), (chan)->channel.exempt, (chan)->exempts, global_exempts, 'e')
#define resetinvites(chan)  resetmasks((chan), (chan)->channel.invite, (chan)->invites, global_invites, 'I')

//static void detect_offense(memberlist*, struct chanset_t *, char *);
/* static int target_priority(struct chanset_t *, memberlist *, int); */
static bool do_op(char *, struct chanset_t *, bool, bool);
static void request_op(struct chanset_t *);
static void request_in(struct chanset_t *);
static void my_setkey(struct chanset_t *, char *);
//static void maybe_revenge(struct chanset_t *, char *, char *, int);
static bool detect_chan_flood(char *, char *, char *, struct chanset_t *, int,
			     char *);
static bool new_mask(masklist *, char *, char *);
static void do_closed_kick(struct chanset_t *, memberlist *);
static char *quickban(struct chanset_t *, char *);
static bool killmember(struct chanset_t *chan, char *nick);
static void check_lonely_channel(struct chanset_t *chan);
static int gotmode(char *, char *);
void unset_im(struct chanset_t* chan);
void detected_drone_flood(struct chanset_t* chan, memberlist*);
static void send_chan_who(int queue, struct chanset_t* chan, bool chain = 0);
#define newban(chan, mask, who)         new_mask((chan)->channel.ban, mask, who)
#define newexempt(chan, mask, who)      new_mask((chan)->channel.exempt, mask, who)
#define newinvite(chan, mask, who)      new_mask((chan)->channel.invite, mask, who)
void resolve_to_member(struct chanset_t *chan, char *nick, char *host);

typedef struct resolvstruct resolv_member;
void resolve_to_rbl(struct chanset_t *chan, const char *host, struct resolvstruct *r = NULL);
static void do_mask(struct chanset_t *chan, masklist *m, char *mask, char Mode);
static void get_channel_masks(struct chanset_t* chan);

#endif /* MAKING_IRC */

void force_join_chan(struct chanset_t* chan, int idx = DP_MODE);
void join_chan(struct chanset_t* chan, int idx = DP_MODE);

int check_bind_authc(char *, Auth *, char *, char *);
void notice_invite(struct chanset_t *, char *, char *, char *, bool);
void real_add_mode(struct chanset_t *, const char, const char, const char *, bool);
#define add_mode(chan, pls, mode, nick) real_add_mode(chan, pls, mode, nick, 0)
#define add_cookie(chan, nick) real_add_mode(chan, '+', 'o', nick, 1)
bool me_op(struct chanset_t *);
bool me_voice(struct chanset_t *);

void check_this_ban(struct chanset_t *, char *, bool);
void check_this_exempt(struct chanset_t *, char *, bool);
void check_this_invite(struct chanset_t *, char *, bool);

inline void check_this_mask(const char type, struct chanset_t *chan, char *mask, bool sticky)
{
  if (channel_active(chan)) {
    if (type == 'b')
      check_this_ban(chan, mask, sticky);
    else if (type == 'e')
      check_this_exempt(chan, mask, sticky);
    else if (type == 'I')
      check_this_invite(chan, mask, sticky);
  }
}

void check_this_user(char *, int, char *);
void raise_limit(struct chanset_t *);
void enforce_closed(struct chanset_t *);
void recheck_channel(struct chanset_t *, int);
void recheck_channel_modes(struct chanset_t *);
void irc_report(int, int);
void flush_modes();
void reset_chan_info(struct chanset_t *);
char *getnick(const char *, struct chanset_t *);

extern int		max_bans, max_exempts, max_invites, max_modes;
extern bool		use_354, include_lk;
extern unsigned int	modesperline;
extern unsigned long my_cookie_counter;
#endif				/* _EGG_MOD_IRC_IRC_H */

