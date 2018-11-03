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


static void check_expired_masks(void);
static void tell_masks(const char type, int idx, bool show_inact, char *match, bool all = 0);
static void get_mode_protect(struct chanset_t *chan, char *s, size_t ssiz);
static void set_mode_protect(struct chanset_t *chan, char *set);
static int count_mask(maskrec *);

#endif				/* MAKING_CHANNELS */

namespace bd {
  class Stream;
  class String;
}

void remove_channel(struct chanset_t *);
void add_chanrec_by_handle(struct userrec *, char *, char *);
void get_handle_chaninfo(char *, char *, char *);
void set_handle_chaninfo(struct userrec *, char *, char *, char *);
struct chanuserrec *get_chanrec(struct userrec *u, char *);
struct chanuserrec *add_chanrec(struct userrec *u, char *);
void del_chanrec(struct userrec *, char *);
void write_bans(bd::Stream&, int);
void write_exempts(bd::Stream&, int);
void write_chans(bd::Stream&, int, bool = 0);
void write_chans_compat(bd::Stream&, int);
bd::String channel_to_string(struct chanset_t* chan, bool force_inactive = 0);
void write_invites(bd::Stream&, int);
bool expired_mask(struct chanset_t *, char *);
void set_handle_laston(char *, struct userrec *, time_t);
int u_delmask(char type, struct chanset_t *c, char *who, int doit);
bool u_addmask(char type, struct chanset_t *, char *, const char *, const char *, time_t, int);
int u_sticky_mask(const maskrec *, const char *) __attribute__((pure));
int u_setsticky_mask(struct chanset_t *, maskrec *, char *, int, const char);
int SplitList(char *, const char *, int *, const char ***);
int channel_modify(char *, struct chanset_t *, int, char **, bool);
int channel_add(char *, const char *, char *, bool = 0);
void clear_channel(struct chanset_t *, bool);
int u_equals_mask(const maskrec *, const char *) __attribute__((pure));
bool u_match_mask(const maskrec *, const char *) __attribute__((pure));
bool ismasked(const masklist *, const char *) __attribute__((pure));
bool ismodeline(const masklist *, const char *) __attribute__((pure));
void channels_report(int, int);
void channels_writeuserfile(bd::Stream&, int = 0);
void rcmd_chans(char *, char *, char *);


/* Macro's here because their functions were replaced by something more
 * generic. <cybah>
 */
#define isbanned(chan, user)    ismasked((chan)->channel.ban, user)
#define isexempted(chan, user)  ismasked((chan)->channel.exempt, user)
#define isinvited(chan, user)   ismasked((chan)->channel.invite, user)

#define ischanban(chan, user)    ismodeline((chan)->channel.ban, user)
#define ischanexempt(chan, user) ismodeline((chan)->channel.exempt, user)
#define ischaninvite(chan, user) ismodeline((chan)->channel.invite, user)
#define ischanmask(type, chan, user) ismodeline(type == 'b' ? (chan)->channel.ban : type == 'e' ? (chan)->channel.exempt : (chan)->channel.invite, user)

#define u_setsticky_ban(chan, host, sticky)     u_setsticky_mask(chan, ((struct chanset_t *)chan) ? ((struct chanset_t *)chan)->bans : global_bans, host, sticky, 'b')
#define u_setsticky_exempt(chan, host, sticky)  u_setsticky_mask(chan, ((struct chanset_t *)chan) ? ((struct chanset_t *)chan)->exempts : global_exempts, host, sticky, 'e')
#define u_setsticky_invite(chan, host, sticky)  u_setsticky_mask(chan, ((struct chanset_t *)chan) ? ((struct chanset_t *)chan)->invites : global_invites, host, sticky, 'I')
#endif				/* _EGG_MOD_CHANNELS_CHANNELS_H */
