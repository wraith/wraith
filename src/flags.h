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


struct flag_record {
  int match;
  int global;
  int udef_global;
  int bot;
  int chan;
  int udef_chan;
};

#define FR_GLOBAL 0x00000001
#define FR_BOT    0x00000002
#define FR_CHAN   0x00000004
#define FR_OR     0x40000000
#define FR_AND    0x20000000
#define FR_ANYWH  0x10000000
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


#define USER_ADMIN         0x00000001 /* a  user is an admin                  */
#define USER_BOT           0x00000002 /* b  user is a bot                     */
#define USER_CHANHUB       0x00000004 /* c  bot is a chanhub    */
#define USER_DEOP          0x00000008 /* d  user is global de-op              */
#define USER_EXEMPT        0x00000010 /* e  exempted from stopnethack         */
#define USER_F             0x00000020 /* f  unused             */
#define USER_G             0x00000040 /* g  unused                            */
#define USER_H             0x00000080 /* h  unused                            */
#define USER_HUBA          0x00000100 /* i  access to HUBS        */
#define USER_CHUBA         0x00000200 /* j  access to CHANHUBS(+c)            */
#define USER_KICK          0x00000400 /* k  user is global auto-kick          */
#define USER_DOLIMIT       0x00000800 /* l  bot sets limit on channel(s)        */
#define USER_MASTER        0x00001000 /* m  user has full bot access          */
#define USER_OWNER         0x00002000 /* n  user is the bot owner             */
#define USER_OP            0x00004000 /* o  user is +o on all channels        */
#define USER_PARTY         0x00008000 /* p  user can CHAT on partyline:*needs (+i or +j)    */
#define USER_QUIET         0x00010000 /* q  user is global de-voice           */
#define USER_R  	   0x00020000 /* r  unused    */
#define USER_S             0x00040000 /* s  unused             */
#define USER_T             0x00080000 /* t  unused             */
#define USER_UPDATEHUB     0x00100000 /* u  bot is the updatehub         */
#define USER_VOICE         0x00200000 /* v  user is +v on all channels        */
#define USER_WASOPTEST     0x00400000 /* w  wasop test needed for stopnethack */
#define USER_NOFLOOD       0x00800000 /* x  user is exempt from flood kicks   */
#define USER_DOVOICE       0x01000000 /* y  bot gives voices                  */
#define USER_UNSHARED      0x02000000 /* z  not shared with sharebots	      */
#define USER_DEFAULT       0x40000000 /* use default-flags                    */

#define bot_hublevel(x) ( ( (x) && (x->flags & USER_BOT) && (get_user(&USERENTRY_BOTADDR, x)) ) ? \
                          ( ((struct bot_addr *) get_user(&USERENTRY_BOTADDR, x))->hublevel ? \
                            ((struct bot_addr *) get_user(&USERENTRY_BOTADDR, x))->hublevel : 999) \
                         : 999)

/* Flags specifically for bots
 */
#define BOT_A         0x00000001	/* a  unused			 */
#define BOT_BOT       0x00000002	/* b  sanity bot flag		 */
#define BOT_C         0x00000004	/* c  unused			 */
#define BOT_D         0x00000008	/* d  unused			 */
#define BOT_E         0x00000010	/* e  unused			 */
#define BOT_F         0x00000020	/* f  unused			 */
#define BOT_GLOBAL    0x00000040	/* g  all channel are shared	 */
#define BOT_HUB       0x00000080	/* h  auto-link to ONE of these
					      bots			 */
#define BOT_ISOLATE   0x00000100	/* i  isolate party line from
					      botnet			 */
#define BOT_J         0x00000200	/* j  unused			 */
#define BOT_K         0x00000400	/* k  unused			 */
#define BOT_LEAF      0x00000800	/* l  may not link other bots	 */
#define BOT_M         0x00001000	/* m  unused			 */
#define BOT_N         0x00002000	/* n  unused			 */
#define BOT_O         0x00004000	/* o  unused			 */
#define BOT_PASSIVE   0x00008000	/* p  share passively with this
					      bot			 */
#define BOT_Q         0x00010000	/* q  unused */
#define BOT_REJECT    0x00020000	/* r  automatically reject
					      anywhere			 */
#define BOT_AGGRESSIVE 0x00040000	/* s  bot shares user files	 */
#define BOT_T         0x00080000	/* t  unused			 */
#define BOT_U         0x00100000	/* u  unused			 */
#define BOT_V         0x00200000	/* v  unused			 */
#define BOT_W         0x00400000	/* w  unused			 */
#define BOT_X         0x00800000	/* x  unused			 */
#define BOT_Y         0x01000000	/* y  unused			 */
#define BOT_Z         0x02000000	/* z  unused			 */
#define BOT_FLAG0     0x00200000	/* 0  user-defined flag #0	 */
#define BOT_FLAG1     0x00400000	/* 1  user-defined flag #1	 */
#define BOT_FLAG2     0x00800000	/* 2  user-defined flag #2	 */
#define BOT_FLAG3     0x01000000	/* 3  user-defined flag #3	 */
#define BOT_FLAG4     0x02000000	/* 4  user-defined flag #4	 */
#define BOT_FLAG5     0x04000000	/* 5  user-defined flag #5	 */
#define BOT_FLAG6     0x08000000	/* 6  user-defined flag #6	 */
#define BOT_FLAG7     0x10000000	/* 7  user-defined flag #7	 */
#define BOT_FLAG8     0x20000000	/* 8  user-defined flag #8	 */
#define BOT_FLAG9     0x40000000	/* 9  user-defined flag #9	 */

#define BOT_SHARE    (BOT_AGGRESSIVE|BOT_PASSIVE)


/* Flag checking macros
 */
#define chan_op(x)			((x).chan & USER_OP)
#define glob_op(x)			((x).global & USER_OP)
#define chan_deop(x)			((x).chan & USER_DEOP)
#define glob_deop(x)			((x).global & USER_DEOP)
#define glob_master(x)			((x).global & USER_MASTER)
#define glob_bot(x)				((x).global & USER_BOT)
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

#define bot_global(x)		(1)
#define bot_chan(x)		((x).chan & BOT_AGGRESSIVE)
#define bot_shared(x)		((x).bot & BOT_SHARE)

#ifndef MAKING_MODS

void get_user_flagrec(struct userrec *, struct flag_record *, const char *);
void set_user_flagrec(struct userrec *, struct flag_record *, const char *);
void break_down_flags(const char *, struct flag_record *, struct flag_record *);
int build_flags(char *, struct flag_record *, struct flag_record *);
int flagrec_eq(struct flag_record *, struct flag_record *);
int flagrec_ok(struct flag_record *, struct flag_record *);
int sanity_check(int);
int chan_sanity_check(int, int);
char geticon(int);

int private(struct flag_record, struct chanset_t *, int);
int chk_op(struct flag_record, struct chanset_t *);
int chk_deop(struct flag_record, struct chanset_t *);
int chk_voice(struct flag_record, struct chanset_t *);
int chk_devoice(struct flag_record, struct chanset_t *);
int ischanhub();
int isupdatehub();
int dovoice(struct chanset_t *);
int dolimit(struct chanset_t *);

#endif				/* MAKING_MODS */

#endif				/* _EGG_FLAGS_H */
