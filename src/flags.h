
/* 
 * flags.h
 * 
 * $Id: flags.h,v 1.5 2000/01/08 21:23:14 per Exp $
 */

/* 
 * Copyright (C) 1997  Robey Pointer
 * Copyright (C) 1999, 2000  Eggheads
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _EGG_FLAGS_H
#define _EGG_FLAGS_H

struct flag_record {
  int match;
  int global;
  int udef_global;
  int chan;
  int udef_chan;
};

#define FR_GLOBAL 0x00000001
#define FR_CHAN   0x00000004
#define FR_OR     0x40000000
#define FR_AND    0x20000000
#define FR_ANYWH  0x10000000
#define FR_ALL    0x0fffffff

/* 
 * userflags:
 *             abcd?fgh?jk?mnopq??tuvwx??
 * + user defined A-Z
 *   unused letters: eilrsyz
 * 
 * botflags:
 *   0123456789ab????ghi??l???p?rs???????
 *   unused letters: cdefjkmnoqtuvwxyz
 * 
 * chanflags:
 *             a??d?fg???k?mno?q?s?uv????
 * + user defined A-Z
 *   unused letters: bchijlprtwxyz
 */

#define ROLE_KICK_MDOP     (role)
#define ROLE_KICK_MEAN     (role)

#define DEFLAG_BADCOOKIE   1
#ifdef G_MANUALOP
#define DEFLAG_MANUALOP    2
#endif
#ifdef G_MEAN
#define DEFLAG_MEAN_DEOP   3
#define DEFLAG_MEAN_KICK   4
#define DEFLAG_MEAN_BAN    5
#endif
#define DEFLAG_MDOP        6

#define USER_VALID    0x00ffffff	/* all USER_ flags in use */
#define CHAN_VALID    0x00757469	/* all flags that can be chan specific */

#ifdef G_BACKUP
#define USER_BACKUPBOT 0x00000001	/* a  backupbot */
#else
#define USER_A        0x00000001	/* a  unused */
#endif
#define USER_BOT      0x00000002	/* b  user is a bot */
#define USER_COMMON   0x00000004	/* c  user is actually a public irc
					 * site */
#define USER_DEOP     0x00000008	/* d  user is global de-op */
#define USER_E        0x00000010	/* e  unused */
#define USER_FRIEND   0x00000020	/* f  user is global friend */
#define USER_GVOICE   0x00000040	/* g  give voice true auto */
#define USER_HIGHLITE 0x00000080	/* h  highlighting (bold) */
#define USER_SU       0x00000100	/* i  Access to .su */
#define USER_HUB      0x00000200	/* j  Access to hubs */
#define USER_KICK     0x00000400	/* k  user is global auto-kick */
#define USER_DOLIMIT  0x00000800	/* l  bot sets limit */
#define USER_MASTER   0x00001000	/* m  user has full bot access */
#define USER_OWNER    0x00002000	/* n  user is the bot owner */
#define USER_OP       0x00004000	/* o  user is +o on all channels */
#define USER_PARTY    0x00008000	/* p  user has party line access */
#define USER_QUIET    0x00010000	/* q  never let 'em get a voice */
#define USER_R        0x00020000	/* r  unused */
#ifdef G_SILENT
#define USER_SILENT   0x00040000	/* s  no partyline chat */
#else
#define USER_S        0x00040000
#endif
#define USER_T        0x00080000	/* t  unused */
#define USER_UNSHARED 0x00100000	/* u  not shared with sharebots */
#define USER_VOICE    0x00200000	/* v  auto-voice on join */
#define USER_DOVOICE  0x00400000	/* w  bot gives auto-voice */
#define USER_X        0x00800000	/* x  unused */
#define USER_Y        0x01000000	/* y  unused */
#define USER_Z        0x02000000	/* z  unused */
#define USER_DEFAULT  0x40000000	/* use default-flags */

#define bot_hublevel(x) ( ( (x) && (x->flags & USER_BOT) && (get_user(&USERENTRY_BOTADDR, x)) ) ? \
                          ( ((struct bot_addr *) get_user(&USERENTRY_BOTADDR, x))->hublevel ? \
                            ((struct bot_addr *) get_user(&USERENTRY_BOTADDR, x))->hublevel : 999) \
			 : 999)

/* flag checking macros */

#define chan_op(x) ((x).chan & USER_OP)
#define glob_op(x) ((x).global & USER_OP)
#define chan_deop(x) ((x).chan & USER_DEOP)
#define glob_deop(x) ((x).global & USER_DEOP)
#define glob_master(x) ((x).global & USER_MASTER)
#define glob_bot(x) ((x).global & USER_BOT)
#ifdef G_BACKUP
#define glob_backupbot(x) ((x).global & USER_BACKUPBOT)
#endif
#define glob_owner(x) ((x).global & USER_OWNER)
#define glob_su(x) ((x).global & USER_SU)
#define glob_hub(x) ((x).global & USER_HUB)
#define chan_dolimit(x) ((x).chan & USER_DOLIMIT)
#define glob_dolimit(x) ((x).global & USER_DOLIMIT)
#define chan_master(x) ((x).chan & USER_MASTER)
#define chan_owner(x) ((x).chan & USER_OWNER)
#define chan_gvoice(x) ((x).chan & USER_GVOICE)
#define glob_gvoice(x) ((x).global & USER_GVOICE)
#define chan_kick(x) ((x).chan & USER_KICK)
#define glob_kick(x) ((x).global & USER_KICK)
#ifdef G_SILENT
#define glob_silent(x) ((x).global & USER_SILENT)
#endif
#define chan_voice(x) ((x).chan & USER_VOICE)
#define glob_voice(x) ((x).global & USER_VOICE)
#define chan_dovoice(x) ((x).chan & USER_DOVOICE)
#define glob_dovoice(x) ((x).global & USER_DOVOICE)
#define chan_quiet(x) ((x).chan & USER_QUIET)
#define glob_quiet(x) ((x).global & USER_QUIET)
#define chan_friend(x) ((x).chan & USER_FRIEND)
#define glob_friend(x) ((x).global & USER_FRIEND)
#define glob_party(x) ((x).global & USER_PARTY)
#define del_glob_xfer(x) ((x).global & DEL_USER_XFER)
#define glob_hilite(x) ((x).global & USER_HIGHLITE)

#define del_bot_chan(x) ((x).chan & BOT_AGGRESSIVE)
#define bot_shared(x) ((x).bot & BOT_SHARE)

#ifndef MAKING_MODS
void get_user_flagrec(struct userrec *, struct flag_record *, char *);
void set_user_flagrec(struct userrec *, struct flag_record *, char *);
void break_down_flags(char *, struct flag_record *, struct flag_record *);
int build_flags(char *, struct flag_record *, struct flag_record *);
int flagrec_eq(struct flag_record *, struct flag_record *);
int flagrec_ok(struct flag_record *, struct flag_record *);
int sanity_check(int);
int chan_sanity_check(int, int);
char geticon(int);

#endif /* MAKING_MODS */

#endif /* _EGG_FLAGS_H */
