#ifndef _EGG_FLAGS_H
#define _EGG_FLAGS_H
struct flag_record
{
  int match;
  int global;
  int udef_global;
  int bot;
  int chan;
  int udef_chan;
};
#define FR_GLOBAL 0x00000001
#define FR_BOT 0x00000002
#define FR_CHAN 0x00000004
#define FR_OR 0x40000000
#define FR_AND 0x20000000
#define FR_ANYWH 0x10000000
#define FR_ALL 0x0fffffff
#define ROLE_KICK_MDOP (role)
#define ROLE_KICK_MEAN (role)
#define DEFLAG_BADCOOKIE 1
#define DEFLAG_MANUALOP 2
#ifdef G_MEAN
#define DEFLAG_MEAN_DEOP 3
#define DEFLAG_MEAN_KICK 4
#define DEFLAG_MEAN_BAN 5
#endif
#define DEFLAG_MDOP 6
#define USER_VALID 0x03ffffff
#define CHAN_VALID 0x03ffffff
#define BOT_VALID 0x7fe689C1
#define USER_ADMIN 0x00000001
#define USER_BOT 0x00000002
#define USER_CHANHUB 0x00000004
#define USER_DEOP 0x00000008
#define USER_EXEMPT 0x00000010
#define USER_FRIEND 0x00000020
#define USER_G 0x00000040
#define USER_HIGHLITE 0x00000080
#define USER_HUBA 0x00000100
#define USER_CHUBA 0x00000200
#define USER_KICK 0x00000400
#define USER_DOLIMIT 0x00000800
#define USER_MASTER 0x00001000
#define USER_OWNER 0x00002000
#define USER_OP 0x00004000
#define USER_PARTY 0x00008000
#define USER_QUIET 0x00010000
#define USER_R 0x00020000
#define USER_SECHUB 0x00040000
#define USER_T 0x00080000
#define USER_UPDATEHUB 0x00100000
#define USER_VOICE 0x00200000
#define USER_WASOPTEST 0x00400000
#define USER_NOFLOOD 0x00800000
#define USER_DOVOICE 0x01000000
#define USER_UNSHARED 0x02000000
#define USER_DEFAULT 0x40000000
#define bot_hublevel(x) ( ( (x) && (x->flags & USER_BOT) && (get_user(&USERENTRY_BOTADDR, x)) ) ? ( ((struct bot_addr *) get_user(&USERENTRY_BOTADDR, x))->hublevel ? ((struct bot_addr *) get_user(&USERENTRY_BOTADDR, x))->hublevel : 999) : 999)
#define BOT_A 0x00000001
#define BOT_BOT 0x00000002
#define BOT_C 0x00000004
#define BOT_D 0x00000008
#define BOT_E 0x00000010
#define BOT_F 0x00000020
#define BOT_GLOBAL 0x00000040
#define BOT_HUB 0x00000080
#define BOT_ISOLATE 0x00000100
#define BOT_J 0x00000200
#define BOT_K 0x00000400
#define BOT_LEAF 0x00000800
#define BOT_M 0x00001000
#define BOT_N 0x00002000
#define BOT_O 0x00004000
#define BOT_PASSIVE 0x00008000
#define BOT_Q 0x00010000
#define BOT_REJECT 0x00020000
#define BOT_AGGRESSIVE 0x00040000
#define BOT_T 0x00080000
#define BOT_U 0x00100000
#define BOT_V 0x00200000
#define BOT_W 0x00400000
#define BOT_X 0x00800000
#define BOT_Y 0x01000000
#define BOT_Z 0x02000000
#define BOT_FLAG0 0x00200000
#define BOT_FLAG1 0x00400000
#define BOT_FLAG2 0x00800000
#define BOT_FLAG3 0x01000000
#define BOT_FLAG4 0x02000000
#define BOT_FLAG5 0x04000000
#define BOT_FLAG6 0x08000000
#define BOT_FLAG7 0x10000000
#define BOT_FLAG8 0x20000000
#define BOT_FLAG9 0x40000000
#define BOT_SHARE (BOT_AGGRESSIVE|BOT_PASSIVE)
#define chan_op(x)	((x).chan & USER_OP)
#define glob_op(x)	((x).global & USER_OP)
#define chan_deop(x)	((x).chan & USER_DEOP)
#define glob_deop(x)	((x).global & USER_DEOP)
#define glob_master(x)	((x).global & USER_MASTER)
#define glob_bot(x)	((x).global & USER_BOT)
#define glob_owner(x)	((x).global & USER_OWNER)
#define chan_master(x)	((x).chan & USER_MASTER)
#define chan_owner(x)	((x).chan & USER_OWNER)
#define chan_kick(x)	((x).chan & USER_KICK)
#define glob_kick(x)	((x).global & USER_KICK)
#define chan_voice(x)	((x).chan & USER_VOICE)
#define glob_voice(x)	((x).global & USER_VOICE)
#define chan_wasoptest(x)	((x).chan & USER_WASOPTEST)
#define glob_wasoptest(x)	((x).global & USER_WASOPTEST)
#define chan_quiet(x)	((x).chan & USER_QUIET)
#define glob_quiet(x)	((x).global & USER_QUIET)
#define chan_friend(x)	((x).chan & USER_FRIEND)
#define glob_friend(x)	((x).global & USER_FRIEND)
#define glob_party(x)	((x).global & USER_PARTY)
#define glob_hilite(x)	((x).global & USER_HIGHLITE)
#define chan_exempt(x)	((x).chan & USER_EXEMPT)
#define glob_exempt(x)	((x).global & USER_EXEMPT)
#define glob_admin(x)	((x).global & USER_ADMIN)
#define glob_huba(x)	((x).global & USER_HUBA)
#define glob_chuba(x)	((x).global & USER_CHUBA)
#define glob_dolimit(x)	((x).global & USER_DOLIMIT)
#define chan_dolimit(x)	((x).chan & USER_DOLIMIT)
#define glob_dovoice(x)	((x).global & USER_DOVOICE)
#define chan_dovoice(x)	((x).chan & USER_DOVOICE)
#define glob_noflood(x)	((x).global & USER_NOFLOOD)
#define chan_noflood(x)	((x).chan & USER_NOFLOOD)
#define glob_chanhub(x)	((x).global & USER_CHANHUB)
#define glob_sechub(x)	((x).global & USER_SECHUB)
#define bot_global(x)	(1)
#define bot_chan(x)	((x).chan & BOT_AGGRESSIVE)
#define bot_shared(x)	((x).bot & BOT_SHARE)
#ifndef MAKING_MODS
void get_user_flagrec (struct userrec *, struct flag_record *, const char *);
void set_user_flagrec (struct userrec *, struct flag_record *, const char *);
void break_down_flags (const char *, struct flag_record *,
		       struct flag_record *);
int build_flags (char *, struct flag_record *, struct flag_record *);
int flagrec_eq (struct flag_record *, struct flag_record *);
int flagrec_ok (struct flag_record *, struct flag_record *);
int sanity_check (int);
int chan_sanity_check (int, int);
char geticon (int);
#endif
#endif
