#ifdef LEAF
/*
 * irc.h -- part of irc.mod
 *
 */

#ifndef _EGG_MOD_IRC_IRC_H
#define _EGG_MOD_IRC_IRC_H

enum { BC_NOCOOKIE = 1, BC_SLACK, BC_HASH };

#define REVENGE_KICK 1		/* Kicked victim	*/
#define REVENGE_DEOP 2		/* Took op		*/

#define PRIO_DEOP 1
#define PRIO_KICK 2

/* For flushmodes */
#define NORMAL          0
#define QUICK           1

#ifdef MAKING_IRC

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

static int check_bind_pubc(char *, char *, char *, struct userrec *, char *, char *);
static char *makecookie(char *, char *);
static int checkcookie(char *, char *, char *);
static bool me_voice(struct chanset_t *);
static bool any_ops(struct chanset_t *);
static char *getchanmode(struct chanset_t *);
static void flush_mode(struct chanset_t *, int);

/* reset(bans|exempts|invites) are now just macros that call resetmasks
 * in order to reduce the code duplication. <cybah>
 */
#define resetbans(chan)	    resetmasks((chan), (chan)->channel.ban, (chan)->bans, global_bans, 'b')
#define resetexempts(chan)  resetmasks((chan), (chan)->channel.exempt, (chan)->exempts, global_exempts, 'e')
#define resetinvites(chan)  resetmasks((chan), (chan)->channel.invite, (chan)->invites, global_invites, 'I')

static void detect_autokick(char *, char *, struct chanset_t *, char *);
/* static int target_priority(struct chanset_t *, memberlist *, int); */
static bool do_op(char *, struct chanset_t *, time_t, bool);
static void request_op(struct chanset_t *);
static void request_in(struct chanset_t *);
static void reset_chan_info(struct chanset_t *);
static void my_setkey(struct chanset_t *, char *);
static void maybe_revenge(struct chanset_t *, char *, char *, int);
static bool detect_chan_flood(char *, char *, char *, struct chanset_t *, int,
			     char *);
static void new_mask(masklist *, char *, char *);
static void doban(struct chanset_t *, memberlist *);
static char *quickban(struct chanset_t *, char *);
static bool real_killmember(struct chanset_t *chan, char *nick, const char *file, int line);
#define killmember(chan, nick)        real_killmember((chan), (nick), __FILE__,__LINE__)
static void check_lonely_channel(struct chanset_t *chan);
static int gotmode(char *, char *);
#define newban(chan, mask, who)         new_mask((chan)->channel.ban, mask, who)
#define newexempt(chan, mask, who)      new_mask((chan)->channel.exempt, mask, who)
#define newinvite(chan, mask, who)      new_mask((chan)->channel.invite, mask, who)

#endif /* MAKING_IRC */

void notice_invite(struct chanset_t *, char *, char *, char *, bool);
void real_add_mode(struct chanset_t *, const char, const char, const char *, bool);
#define add_mode(chan, pls, mode, nick) real_add_mode(chan, pls, mode, nick, 0)
#define add_cookie(chan, nick) real_add_mode(chan, '+', 'o', nick, 1)
bool me_op(struct chanset_t *);
void check_this_ban(struct chanset_t *, char *, bool);
void check_this_exempt(struct chanset_t *, char *, bool);
void check_this_invite(struct chanset_t *, char *, bool);
void check_this_user(char *, int, char *);
void raise_limit(struct chanset_t *);
void enforce_closed(struct chanset_t *);
void recheck_channel(struct chanset_t *, int);
void recheck_channel_modes(struct chanset_t *);
void irc_report(int, int);
void flush_modes();

extern int		max_bans, max_exempts, max_invites, max_modes;
extern bool		use_354;
extern unsigned int	modesperline;
#endif				/* _EGG_MOD_IRC_IRC_H */

#endif /* LEAF */
