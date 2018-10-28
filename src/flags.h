/*
 * flags.h
 *
 */

#ifndef _EGG_FLAGS_H
#define _EGG_FLAGS_H

#include "chan.h"

/* For privchan() checking */
#define PRIV_OP 1
#define PRIV_VOICE 2


typedef uint64_t flag_t;

extern flag_t FLAG[128];

struct flag_record {
  flag_t match;
  flag_t global;
  flag_t chan;
  char bot;
};

#define FR_GLOBAL 0x00000001
#define FR_CHAN   0x00000002
#define FR_BOT    0x00000004
#define FR_ANYWH  0x10000000
#define FR_ANYCH  0x20000000
#define FR_AND    0x40000000
#define FR_OR     0x80000000

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

enum deflag_event_t {
  DEFLAG_EVENT_BADCOOKIE = 1,
  DEFLAG_EVENT_MANUALOP,
  DEFLAG_EVENT_REVENGE_DEOP,
  DEFLAG_EVENT_REVENGE_KICK,
  DEFLAG_EVENT_REVENGE_BAN,
  DEFLAG_EVENT_MDOP,
  DEFLAG_EVENT_MOP,
};

struct rolecount {
  const char* name;
  short role;
  int count;
};

extern struct rolecount role_counts[];

#define ROLE_VOICE    BIT0
#define ROLE_FLOOD    BIT1
#define ROLE_OP       BIT2
#define ROLE_DEOP     BIT3
#define ROLE_KICK     BIT4
#define ROLE_BAN      BIT5
#define ROLE_TOPIC    BIT6
#define ROLE_LIMIT    BIT7
#define ROLE_RESOLV   BIT8
#define ROLE_REVENGE  BIT9
#define ROLE_CHANMODE BIT10
#define ROLE_PROTECT  BIT11
#define ROLE_INVITE   BIT12

#define USER_DEFAULT	0

#define USER_ADMIN	FLAG[(int) 'a']
#define USER_HUBA	FLAG[(int) 'i']
#define USER_CHUBA	FLAG[(int) 'j']
#define USER_PARTY	FLAG[(int) 'p']
#define USER_MASTER	FLAG[(int) 'm']
#define USER_OWNER	FLAG[(int) 'n']

#define USER_AUTOOP	FLAG[(int) 'O']

#define USER_DEOP	FLAG[(int) 'd']
#define USER_KICK	FLAG[(int) 'k']
#define USER_OP		FLAG[(int) 'o']
#define USER_QUIET	FLAG[(int) 'q']
#define USER_VOICE	FLAG[(int) 'v']
#define USER_NOFLOOD	FLAG[(int) 'x']
#define USER_WASOPTEST	FLAG[(int) 'w']

#define CHAN_VALID (flag_t) USER_DEOP|USER_KICK|USER_OP|USER_QUIET|USER_VOICE|USER_NOFLOOD|USER_WASOPTEST

#define USER_CHAN_VALID (flag_t) CHAN_VALID|USER_AUTOOP|USER_MASTER|USER_OWNER
#define USER_VALID (flag_t) USER_ADMIN|USER_HUBA|USER_CHUBA|USER_PARTY|USER_CHAN_VALID

#define BOT_BACKUP	FLAG[(int) 'B']
#define BOT_CHANHUB	FLAG[(int) 'c']
#define BOT_FLOODBOT	FLAG[(int) 'f'] 
#define BOT_UPDATEHUB	FLAG[(int) 'u']
#define BOT_DORESOLV	FLAG[(int) 'r']
#define BOT_DOLIMIT	FLAG[(int) 'l']
#define BOT_DOVOICE	FLAG[(int) 'y']

#define BOT_CHAN_VALID (flag_t) CHAN_VALID|BOT_CHANHUB|BOT_DOLIMIT|BOT_DOVOICE|BOT_DORESOLV|BOT_BACKUP|BOT_FLOODBOT
#define BOT_VALID  (flag_t) BOT_CHAN_VALID|BOT_UPDATEHUB


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
#define glob_hilite(x)                 ((x).global & USER_HIGHLITE)
#define glob_admin(x)                  ((x).global & USER_ADMIN)
#define glob_huba(x)                   ((x).global & USER_HUBA)
#define glob_chuba(x)                  ((x).global & USER_CHUBA)
#define glob_noflood(x)                        ((x).global & USER_NOFLOOD)
#define chan_noflood(x)                        ((x).chan & USER_NOFLOOD)

#define glob_dolimit(x)                        ((x).global & BOT_DOLIMIT)
#define chan_dolimit(x)                        ((x).chan & BOT_DOLIMIT)
#define glob_dovoice(x)                        ((x).global & BOT_DOVOICE)
#define chan_dovoice(x)                        ((x).chan & BOT_DOVOICE)
#define glob_chanhub(x)                        ((x).global & BOT_CHANHUB)
#define glob_doresolv(x)                        ((x).global & BOT_DORESOLV)
#define chan_doresolv(x)                        ((x).chan & BOT_DORESOLV)
#define glob_backup(x)				((x).global & BOT_BACKUP)
#define chan_backup(x)				((x).chan & BOT_BACKUP)
#define glob_doflood(x)				((x).global & BOT_FLOODBOT)
#define chan_doflood(x)				((x).chan & BOT_FLOODBOT)

void init_flags(void);
void get_user_flagrec(const struct userrec *, struct flag_record *, const char *, const struct chanset_t* = NULL);
void set_user_flagrec(struct userrec *, struct flag_record *, const char *);
void break_down_flags(const char *, struct flag_record *, struct flag_record *);
int build_flags(char *, struct flag_record *, struct flag_record *);
int flagrec_eq(const struct flag_record *, const struct flag_record *)
  __attribute__((pure));
int flagrec_ok(const struct flag_record *, const struct flag_record *)
  __attribute__((pure));
flag_t sanity_check(flag_t, int) __attribute__((const));
flag_t chan_sanity_check(flag_t, int) __attribute__((const));
char geticon(int);
int privchan(const struct flag_record, const struct chanset_t *, int) __attribute__((pure));
#define chk_op(fr, chan) real_chk_op(fr, chan, 1)
int real_chk_op(const struct flag_record, const struct chanset_t *, bool) __attribute__((pure));
int chk_autoop(const memberlist *, const struct flag_record, const struct chanset_t *) __attribute__((pure));
#define chk_deop(fr, chan) real_chk_deop(fr, chan, 1)
int real_chk_deop(const struct flag_record, const struct chanset_t *, bool) __attribute__((pure));
int chk_voice(const memberlist *, const struct flag_record, const struct chanset_t *) __attribute__((pure));
#define chk_noflood(fr) (chan_noflood(fr) || glob_noflood(fr))
#define chk_devoice(fr) ((chan_quiet(fr) || (glob_quiet(fr) && !chan_voice(fr))) ? 1 : 0)
#define isupdatehub() ((conf.bot->hub && conf.bot->u && (conf.bot->u->flags & BOT_UPDATEHUB)) ? 1 : 0)
#define ischanhub() ((!conf.bot->hub && conf.bot->u && (conf.bot->u->flags & BOT_CHANHUB)) ? 1 : 0)
int doresolv(const struct chanset_t *);
int dovoice(const struct chanset_t *);
int doflood(const struct chanset_t *);
int dolimit(const struct chanset_t *);
int whois_access(struct userrec *, struct userrec *);
homechan_user_t homechan_user_translate(const char *) __attribute__((pure));
void deflag_user(struct userrec *, deflag_event_t, const char *, const struct chanset_t *);
deflag_t deflag_translate(const char *) __attribute__((pure));


#endif				/* _EGG_FLAGS_H */
