/*
 * channels.h -- part of channels.mod
 *
 */

#ifndef _EGG_MOD_CHANNELS_CHANNELS_H
#define _EGG_MOD_CHANNELS_CHANNELS_H

#define MASKREASON_MAX	307	/* Max length of ban/invite/exempt/etc.
				   reasons.				*/
#define MASKREASON_LEN	(MASKREASON_MAX + 1)


#ifdef MAKING_CHANNELS

#define PLSMNS(x) (x ? '+' : '-')

static void tell_bans(int idx, int show_inact, char *match);

static void check_expired_bans(void);
static void check_expired_exempts(void);
static void check_expired_invites(void);
static void tell_exempts (int idx, int show_inact, char * match);
static void tell_invites (int idx, int show_inact, char * match);
static void get_mode_protect(struct chanset_t *chan, char *s);
static void set_mode_protect(struct chanset_t *chan, char *set);
inline static int chanset_unlink(struct chanset_t *chan);

#endif				/* MAKING_CHANNELS */

void remove_channel(struct chanset_t *);
void add_chanrec_by_handle(struct userrec *, char *, char *);
void get_handle_chaninfo(char *, char *, char *);
void set_handle_chaninfo(struct userrec *, char *, char *, char *);
struct chanuserrec *get_chanrec(struct userrec *u, char *);
struct chanuserrec *add_chanrec(struct userrec *u, char *);
void del_chanrec(struct userrec *, char *);
int write_bans(FILE *, int);
int write_config (FILE *, int);
int write_exempts (FILE *, int);
int write_chans (FILE *, int);
int write_invites (FILE *, int);
int expired_mask(struct chanset_t *, char *);
void set_handle_laston(char *, struct userrec *, time_t);
int u_delmask(char type, struct chanset_t *c, char *who, int doit);
int u_addmask(char type, struct chanset_t *, char *, char *, char *, time_t, int);
int u_sticky_mask(maskrec *, char *);
int u_setsticky_mask(struct chanset_t *, maskrec *, char *, int, char *);
int SplitList(char *, const char *, int *, const char ***);
int channel_modify(char *, struct chanset_t *, int, char **);
int channel_add(char *, char *, char *);
void check_should_lock();
void clear_channel(struct chanset_t *, int);
int u_equals_mask(maskrec *, char *);
int u_match_mask(struct maskrec *, char *);
int ismasked(masklist *, char *);
int ismodeline(masklist *, char *);
void channels_report(int, int);
#ifdef HUB
void channels_writeuserfile();
#endif /* HUB */

extern char		glob_chanset[], cfg_glob_chanset[];

/* Macro's here because their functions were replaced by something more
 * generic. <cybah>
 */
#define isbanned(chan, user)    ismasked((chan)->channel.ban, user)
#define isexempted(chan, user)  ismasked((chan)->channel.exempt, user)
#define isinvited(chan, user)   ismasked((chan)->channel.invite, user)

#define ischanban(chan, user)    ismodeline((chan)->channel.ban, user)
#define ischanexempt(chan, user) ismodeline((chan)->channel.exempt, user)
#define ischaninvite(chan, user) ismodeline((chan)->channel.invite, user)

#define u_setsticky_ban(chan, host, sticky)     u_setsticky_mask(chan, ((struct chanset_t *)chan) ? ((struct chanset_t *)chan)->bans : global_bans, host, sticky, "s")
#define u_setsticky_exempt(chan, host, sticky)  u_setsticky_mask(chan, ((struct chanset_t *)chan) ? ((struct chanset_t *)chan)->exempts : global_exempts, host, sticky, "se")
#define u_setsticky_invite(chan, host, sticky)  u_setsticky_mask(chan, ((struct chanset_t *)chan) ? ((struct chanset_t *)chan)->invites : global_invites, host, sticky, "sInv")
#endif				/* _EGG_MOD_CHANNELS_CHANNELS_H */
