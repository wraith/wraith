#ifdef LEAF
/*
 * irc.h -- part of irc.mod
 *
 */

#ifndef _EGG_MOD_IRC_IRC_H
#define _EGG_MOD_IRC_IRC_H

#define REVENGE_KICK 1		/* Kicked victim	*/
#define REVENGE_DEOP 2		/* Took op		*/

#define PRIO_DEOP 1
#define PRIO_KICK 2

/* For flushmodes */
#define NORMAL          0
#define QUICK           1

#ifdef MAKING_IRC
#ifdef S_AUTH
static int check_bind_pubc(char *, char *, char *, struct userrec *, char *, char *);
#endif /* S_AUTH */
static void makeopline(struct chanset_t *, char *, char *);
static int me_op(struct chanset_t *);
static int me_voice(struct chanset_t *);
static int any_ops(struct chanset_t *);
static char *getchanmode(struct chanset_t *);
static void flush_mode(struct chanset_t *, int);

/* reset(bans|exempts|invites) are now just macros that call resetmasks
 * in order to reduce the code duplication. <cybah>
 */
#define resetbans(chan)	    resetmasks((chan), (chan)->channel.ban,	\
				       (chan)->bans, global_bans, 'b')
#define resetexempts(chan)  resetmasks((chan), (chan)->channel.exempt,	\
				       (chan)->exempts, global_exempts, 'e')
#define resetinvites(chan)  resetmasks((chan), (chan)->channel.invite,	\
				       (chan)->invites, global_invites, 'I')

static void detect_autokick(char *, char *, struct chanset_t *, char *);
/* static int target_priority(struct chanset_t *, memberlist *, int); */
static int do_op(char *, struct chanset_t *, int);
static void request_op(struct chanset_t *);
static void request_in(struct chanset_t *);
static void reset_chan_info(struct chanset_t *);
static void recheck_channel(struct chanset_t *, int);
static void my_setkey(struct chanset_t *, char *);
static void maybe_revenge(struct chanset_t *, char *, char *, int);
static int detect_chan_flood(char *, char *, char *, struct chanset_t *, int,
			     char *);
static void newmask(masklist *, char *, char *);
static void doban(struct chanset_t *, memberlist *);
static char *quickban(struct chanset_t *, char *);
static void got_op(struct chanset_t *chan, char *nick, char *from, char *who,
 		   struct userrec *opu, struct flag_record *opper);
static int real_killmember(struct chanset_t *chan, char *nick, const char *file, int line);
#define killmember(chan, nick)        real_killmember((chan), (nick), __FILE__,__LINE__)
static void check_lonely_channel(struct chanset_t *chan);
static int gotmode(char *, char *);
void raise_limit(struct chanset_t *);
#define newban(chan, mask, who)         newmask((chan)->channel.ban, mask, who)
#define newexempt(chan, mask, who)      newmask((chan)->channel.exempt, mask, \
						who)
#define newinvite(chan, mask, who)      newmask((chan)->channel.invite, mask, \
						who)

#else
/* 4 - 7 */
/* 8 - 11 */
/* 12 - 15 */
/* recheck_channel is here */
/* 16 - 19 */
#define me_op ((int(*)(struct chanset_t *))irc_funcs[16])
/* recheck_channel_modes is here */
/* do_channel_part is here. */
/* 20 - 23 */
#define check_this_ban ((void (*)(struct chanset_t *, char *, int))irc_funcs[20])
#define check_this_user ((void(*)(char *, int, char *))irc_funcs[21])
#define me_voice ((int(*)(struct chanset_t *))irc_funcs[22])
#define raise_limit ((void (*)(struct chanset_t *))irc_funcs[23])
#define enforce_closed ((void (*)(struct chanset_t *))irc_funcs[24])
//#define getchanmode ((char *(*)(struct chanset_t *))irc_funcs[23])

#endif				/* MAKING_IRC */

#endif				/* _EGG_MOD_IRC_IRC_H */

#endif /* LEAF */
