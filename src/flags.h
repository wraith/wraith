/*
 * flags.h
 *
 */

#ifndef _EGG_FLAGS_H
#define _EGG_FLAGS_H

#include "chan.h"

/* For private() checking */
#define PRIV_OP 1
#define PRIV_VOICE 2


typedef uint64_t flag_t;

extern flag_t	FLAG[128];

struct flag_record {
  flag_t match;
  flag_t global;
  flag_t chan;
  char bot;
};

#define FR_GLOBAL 0x00000001
#define FR_CHAN   0x00000002
#define FR_ANYWH  0x10000000
#define FR_AND    0x20000000
#define FR_OR     0x40000000

/*
 * userflags:
 *   abcdefgh?jklmnopqr?tuvwxy?
 *   + user defined A-Z
 *   unused letters: isz
 *
 * botflags:
 *   0123456789ab????ghi??l???p?rs???????
 *   unused letters: cdefjkmnoqtuvwxyz
 *
 * chanflags:
 *   a??defg???klmno?qr??uv?xyz
 *   + user defined A-Z
 *   unused letters: bchijpstw
 */

#define DEFLAG_BADCOOKIE   1
#define DEFLAG_MANUALOP    2
#ifdef G_MEAN
#  define DEFLAG_MEAN_DEOP   3
#  define DEFLAG_MEAN_KICK   4
#  define DEFLAG_MEAN_BAN    5
#endif /* G_MEAN */
#define DEFLAG_MDOP        6
#define DEFLAG_MOP	   7

#define USER_VALID (flag_t) 0xfffffffffffff
#define CHAN_VALID (flag_t) 0xfffffffffffff

#define USER_ADMIN	FLAG['a']
/* FIXME; REMOVE AFTER 1.2 */
#define USER_BOT	FLAG['b']
#define USER_CHANHUB	FLAG['c']
#define USER_DEOP	FLAG['d']
#define USER_EXEMPT	FLAG['e']
#define USER_HUBA	FLAG['i']
#define USER_CHUBA	FLAG['j']
#define USER_KICK	FLAG['k']
#define USER_DOLIMIT	FLAG['l']
#define USER_MASTER	FLAG['m']
#define USER_OWNER	FLAG['n']
#define USER_OP		FLAG['o']
#define USER_AUTOOP	FLAG['O']
#define USER_PARTY	FLAG['p']
#define USER_QUIET	FLAG['q']
#define USER_UPDATEHUB	FLAG['u']
#define USER_VOICE	FLAG['v']
#define USER_WASOPTEST	FLAG['w']
#define USER_NOFLOOD	FLAG['x']
#define USER_DOVOICE	FLAG['y']
#define USER_DEFAULT	0


#define bot_hublevel(x) ( ( (x) && x->bot && (get_user(&USERENTRY_BOTADDR, x)) ) ? \
                          ( ((struct bot_addr *) get_user(&USERENTRY_BOTADDR, x))->hublevel ? \
                            ((struct bot_addr *) get_user(&USERENTRY_BOTADDR, x))->hublevel : 999) \
                         : 999)
#define glob_bot(x)		       ((x).bot)

/* Flag checking macros
 */
#define chan_op(x)                     ((x).chan & USER_OP)
#define glob_op(x)                     ((x).global & USER_OP)
#define chan_autoop(x)                 ((x).chan & USER_AUTOOP)
#define glob_autoop(x)                 ((x).global & USER_AUTOOP)
#define chan_deop(x)                   ((x).chan & USER_DEOP)
#define glob_deop(x)                   ((x).global & USER_DEOP)
#define glob_master(x)                 ((x).global & USER_MASTER)
#define glob_owner(x)                  ((x).global & USER_OWNER)
#define chan_master(x)                 ((x).chan & USER_MASTER)
#define chan_owner(x)                  ((x).chan & USER_OWNER)
#define chan_kick(x)                   ((x).chan & USER_KICK)
#define glob_kick(x)                   ((x).global & USER_KICK)
#define chan_voice(x)                  ((x).chan & USER_VOICE)
#define glob_voice(x)                  ((x).global & USER_VOICE)
#define chan_wasoptest(x)              ((x).chan & USER_WASOPTEST)
#define glob_wasoptest(x)              ((x).global & USER_WASOPTEST)
#define chan_quiet(x)                  ((x).chan & USER_QUIET)
#define glob_quiet(x)                  ((x).global & USER_QUIET)
#define glob_party(x)                  ((x).global & USER_PARTY)
#define glob_hilite(x)                         ((x).global & USER_HIGHLITE)
#define chan_exempt(x)                 ((x).chan & USER_EXEMPT)
#define glob_exempt(x)                 ((x).global & USER_EXEMPT)
#define glob_admin(x)                  ((x).global & USER_ADMIN)
#define glob_huba(x)                   ((x).global & USER_HUBA)
#define glob_chuba(x)                  ((x).global & USER_CHUBA)
#define glob_dolimit(x)                        ((x).global & USER_DOLIMIT)
#define chan_dolimit(x)                        ((x).chan & USER_DOLIMIT)
#define glob_dovoice(x)                        ((x).global & USER_DOVOICE)
#define chan_dovoice(x)                        ((x).chan & USER_DOVOICE)
#define glob_noflood(x)                        ((x).global & USER_NOFLOOD)
#define chan_noflood(x)                        ((x).chan & USER_NOFLOOD)
#define glob_chanhub(x)                        ((x).global & USER_CHANHUB)

void init_flags();
void get_user_flagrec(struct userrec *, struct flag_record *, const char *);
void set_user_flagrec(struct userrec *, struct flag_record *, const char *);
void break_down_flags(const char *, struct flag_record *, struct flag_record *);
int build_flags(char *, struct flag_record *, struct flag_record *);
int flagrec_eq(struct flag_record *, struct flag_record *);
int flagrec_ok(struct flag_record *, struct flag_record *);
flag_t sanity_check(flag_t, int);
flag_t chan_sanity_check(flag_t);
char geticon(int);

inline int private(struct flag_record, struct chanset_t *, int);
inline int chk_op(struct flag_record, struct chanset_t *);
inline int chk_autoop(struct flag_record, struct chanset_t *);
inline int chk_deop(struct flag_record);
inline int chk_voice(struct flag_record, struct chanset_t *);
inline int chk_devoice(struct flag_record);
inline int chk_noflood(struct flag_record);
inline int ischanhub();
inline int isupdatehub();
int dovoice(struct chanset_t *);
int dolimit(struct chanset_t *);
int whois_access(struct userrec *, struct userrec *);

#endif				/* _EGG_FLAGS_H */
