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


typedef long flag_t;

struct flag_record {
  flag_t match;
  flag_t global;
  flag_t  bot;
  flag_t chan;
};

#define FR_GLOBAL 0x00000001
#define FR_BOT    0x00000002
#define FR_CHAN   0x00000004
#define FR_ANYWH  0x10000000
#define FR_AND    0x20000000
#define FR_OR     0x40000000
#define FR_ALL    0x0fffffff

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

#define ROLE_KICK_MDOP     (role)
#define ROLE_KICK_MEAN     (role)

#define DEFLAG_BADCOOKIE   1
#define DEFLAG_MANUALOP    2
#ifdef G_MEAN
#  define DEFLAG_MEAN_DEOP   3
#  define DEFLAG_MEAN_KICK   4
#  define DEFLAG_MEAN_BAN    5
#endif /* G_MEAN */
#define DEFLAG_MDOP        6
#define DEFLAG_MOP	   7

#define USER_VALID 0x03ffffff	/* all USER_ flags in use              */
#define CHAN_VALID 0x03ffffff	/* all flags that can be chan specific */
#define BOT_VALID  0x7fe689C1 /* all BOT_ flags in use               */


#define USER_ADMIN         BIT0  /* a  user is an admin                  */
#define USER_BOT           BIT1  /* b  user is a bot                     */
#define USER_CHANHUB       BIT2  /* c  bot is a chanhub    */
#define USER_DEOP          BIT3  /* d  user is global de-op              */
#define USER_EXEMPT        BIT4  /* e  exempted from stopnethack         */
#define USER_F             BIT5  /* f  unused             */
#define USER_G             BIT6  /* g  unused                            */
#define USER_H             BIT7  /* h  unused                            */
#define USER_HUBA          BIT8  /* i  access to HUBS        */
#define USER_CHUBA         BIT9  /* j  access to CHANHUBS(+c)            */
#define USER_KICK          BIT10 /* k  user is global auto-kick          */
#define USER_DOLIMIT       BIT11 /* l  bot sets limit on channel(s)        */
#define USER_MASTER        BIT12 /* m  user has full bot access          */
#define USER_OWNER         BIT13 /* n  user is the bot owner             */
#define USER_OP            BIT14 /* o  user is +o on all channels        */
#define USER_PARTY         BIT15 /* p  user can CHAT on partyline:*needs (+i or +j)    */
#define USER_QUIET         BIT16 /* q  user is global de-voice           */
#define USER_R  	   BIT17 /* r  unused    */
#define USER_S             BIT18 /* s  unused             */
#define USER_T             BIT19 /* t  unused             */
#define USER_UPDATEHUB     BIT20 /* u  bot is the updatehub         */
#define USER_VOICE         BIT21 /* v  user is +v on all channels        */
#define USER_WASOPTEST     BIT22 /* w  wasop test needed for stopnethack */
#define USER_NOFLOOD       BIT23 /* x  user is exempt from flood kicks   */
#define USER_DOVOICE       BIT24 /* y  bot gives voices                  */
#define USER_UNSHARED      BIT25 /* z  not shared with sharebots	      */
#define USER_DEFAULT       BIT26 /* use default-flags                    */

#define bot_hublevel(x) ( ( (x) && (x->flags & USER_BOT) && (get_user(&USERENTRY_BOTADDR, x)) ) ? \
                          ( ((struct bot_addr *) get_user(&USERENTRY_BOTADDR, x))->hublevel ? \
                            ((struct bot_addr *) get_user(&USERENTRY_BOTADDR, x))->hublevel : 999) \
                         : 999)

/* Flag checking macros
 */
#define chan_op(x)			((x).chan & USER_OP)
#define glob_op(x)			((x).global & USER_OP)
#define chan_deop(x)			((x).chan & USER_DEOP)
#define glob_deop(x)			((x).global & USER_DEOP)
#define glob_master(x)			((x).global & USER_MASTER)
#define glob_bot(x)			((x).global & USER_BOT)
#define glob_owner(x)			((x).global & USER_OWNER)
#define chan_master(x)			((x).chan & USER_MASTER)
#define chan_owner(x)			((x).chan & USER_OWNER)
#define chan_kick(x)			((x).chan & USER_KICK)
#define glob_kick(x)			((x).global & USER_KICK)
#define chan_voice(x)			((x).chan & USER_VOICE)
#define glob_voice(x)			((x).global & USER_VOICE)
#define chan_wasoptest(x)		((x).chan & USER_WASOPTEST)
#define glob_wasoptest(x)		((x).global & USER_WASOPTEST)
#define chan_quiet(x)			((x).chan & USER_QUIET)
#define glob_quiet(x)			((x).global & USER_QUIET)
#define glob_party(x)			((x).global & USER_PARTY)
#define glob_hilite(x) 			((x).global & USER_HIGHLITE)
#define chan_exempt(x)			((x).chan & USER_EXEMPT)
#define glob_exempt(x)			((x).global & USER_EXEMPT)
#define glob_admin(x)			((x).global & USER_ADMIN)
#define glob_huba(x)			((x).global & USER_HUBA)
#define glob_chuba(x)			((x).global & USER_CHUBA)
#define glob_dolimit(x)			((x).global & USER_DOLIMIT)
#define chan_dolimit(x)			((x).chan & USER_DOLIMIT)
#define glob_dovoice(x)			((x).global & USER_DOVOICE)
#define chan_dovoice(x)			((x).chan & USER_DOVOICE)
#define glob_noflood(x)			((x).global & USER_NOFLOOD)
#define chan_noflood(x)			((x).chan & USER_NOFLOOD)
#define glob_chanhub(x)			((x).global & USER_CHANHUB)

void get_user_flagrec(struct userrec *, struct flag_record *, const char *);
void set_user_flagrec(struct userrec *, struct flag_record *, const char *);
void break_down_flags(const char *, struct flag_record *, struct flag_record *);
int build_flags(char *, struct flag_record *, struct flag_record *);
int flagrec_eq(struct flag_record *, struct flag_record *);
int flagrec_ok(struct flag_record *, struct flag_record *);
int sanity_check(flag_t);
int chan_sanity_check(int, int);
char geticon(int);

int private(struct flag_record, struct chanset_t *, int);
int chk_op(struct flag_record, struct chanset_t *);
int chk_deop(struct flag_record, struct chanset_t *);
int chk_voice(struct flag_record, struct chanset_t *);
int chk_devoice(struct flag_record, struct chanset_t *);
int chk_noflood(struct flag_record, struct chanset_t *);
int ischanhub();
int isupdatehub();
int dovoice(struct chanset_t *);
int dolimit(struct chanset_t *);
int whois_access(struct userrec *, struct userrec *);

#endif				/* _EGG_FLAGS_H */
