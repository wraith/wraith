#ifndef _EGG_MOD_CHANNELS_CHANNELS_H
#define _EGG_MOD_CHANNELS_CHANNELS_H
#define UDEF_FLAG 1
#define UDEF_INT 2
#define MASKREASON_MAX	307
#define MASKREASON_LEN	(MASKREASON_MAX + 1)
#ifdef MAKING_CHANNELS
struct udef_chans
{
  struct udef_chans *next;
  char *chan;
  int value;
};
struct udef_struct
{
  struct udef_struct *next;
  char *name;
  int defined;
  int type;
  struct udef_chans *values;
};
#define PLSMNS(x) (x ? '+' : '-')
static void del_chanrec (struct userrec *u, char *);
static struct chanuserrec *get_chanrec (struct userrec *u, char *chname);
static struct chanuserrec *add_chanrec (struct userrec *u, char *chname);
static void add_chanrec_by_handle (struct userrec *bu, char *hand,
				   char *chname);
static void get_handle_chaninfo (char *handle, char *chname, char *s);
static void set_handle_chaninfo (struct userrec *bu, char *handle,
				 char *chname, char *info);
static void set_handle_laston (char *chan, struct userrec *u, time_t n);
static int u_sticky_mask (maskrec * u, char *uhost);
static int u_setsticky_mask (struct chanset_t *chan, maskrec * m, char *uhost,
			     int sticky, char *botcmd);
static int u_equals_mask (maskrec * u, char *uhost);
static int u_match_mask (struct maskrec *rec, char *mask);
#ifdef S_IRCNET
static int u_delexempt (struct chanset_t *c, char *who, int doit);
static int u_addexempt (struct chanset_t *chan, char *exempt, char *from,
			char *note, time_t expire_time, int flags);
static int u_delinvite (struct chanset_t *c, char *who, int doit);
static int u_addinvite (struct chanset_t *chan, char *invite, char *from,
			char *note, time_t expire_time, int flags);
#endif
static int u_delban (struct chanset_t *c, char *who, int doit);
static int u_addban (struct chanset_t *chan, char *ban, char *from,
		     char *note, time_t expire_time, int flags);
static void tell_bans (int idx, int show_inact, char *match);
static int write_bans (FILE * f, int idx);
static int write_config (FILE * f, int idx);
static void check_expired_bans (void);
static void switch_static (void);
#ifdef S_IRCNET
static void tell_exempts (int idx, int show_inact, char *match);
static int write_exempts (FILE * f, int idx);
#endif
static int write_chans (FILE * f, int idx);
#ifdef S_IRCNET
static void check_expired_exempts (void);
static void tell_invites (int idx, int show_inact, char *match);
static int write_invites (FILE * f, int idx);
static void check_expired_invites (void);
#endif
static void clear_channel (struct chanset_t *, int);
static void get_mode_protect (struct chanset_t *chan, char *s);
static void set_mode_protect (struct chanset_t *chan, char *set);
static int ismasked (masklist * m, char *user);
static int ismodeline (masklist * m, char *user);
static int tcl_channel_modify (Tcl_Interp * irp, struct chanset_t *chan,
			       int items, char **item);
static int tcl_channel_add (Tcl_Interp * irp, char *, char *);
static char *convert_element (char *src, char *dst);
static int expmem_udef (struct udef_struct *);
static int expmem_udef_chans (struct udef_chans *);
static void free_udef (struct udef_struct *);
static void free_udef_chans (struct udef_chans *);
static int getudef (struct udef_chans *, char *);
static void initudef (int type, char *, int);
static void setudef (struct udef_struct *, char *, int);
static void remove_channel (struct chanset_t *);
static int ngetudef (char *, char *);
static int expired_mask (struct chanset_t *chan, char *who);
inline static int chanset_unlink (struct chanset_t *chan);
#else
#define u_setsticky_mask ((int (*)(struct chanset_t *, maskrec *, char *, int, char *))channels_funcs[4])
#define u_delban ((int (*)(struct chanset_t *, char *, int))channels_funcs[5])
#define u_addban ((int (*)(struct chanset_t *, char *, char *, char *, time_t, int))channels_funcs[6])
#define write_bans ((int (*)(FILE *, int))channels_funcs[7])
#define get_chanrec ((struct chanuserrec *(*)(struct userrec *, char *))channels_funcs[8])
#define add_chanrec ((struct chanuserrec *(*)(struct userrec *, char *))channels_funcs[9])
#define del_chanrec ((void (*)(struct userrec *, char *))channels_funcs[10])
#define set_handle_chaninfo ((void (*)(struct userrec *, char *, char *, char *))channels_funcs[11])
#define channel_malloc(x) ((void *(*)(int, char *, int))channels_funcs[12])(x,__FILE__,__LINE__)
#define u_match_mask ((int (*)(maskrec *, char *))channels_funcs[13])
#define u_equals_mask ((int (*)(maskrec *, char *))channels_funcs[14])
#define clear_channel ((void (*)(struct chanset_t *, int))channels_funcs[15])
#define set_handle_laston ((void (*)(char *,struct userrec *,time_t))channels_funcs[16])
#define use_info (*(int *)(channels_funcs[18]))
#define get_handle_chaninfo ((void (*)(char *, char *, char *))channels_funcs[19])
#define u_sticky_mask ((int (*)(maskrec *, char *))channels_funcs[20])
#define ismasked ((int (*)(masklist *, char *))channels_funcs[21])
#define add_chanrec_by_handle ((void (*)(struct userrec *, char *, char *))channels_funcs[22])
#define u_delexempt ((int (*)(struct chanset_t *, char *, int))channels_funcs[29])
#define u_addexempt ((int (*)(struct chanset_t *, char *, char *, char *, time_t, int))channels_funcs[30])
#ifdef S_IRCNET
#define u_delinvite ((int (*)(struct chanset_t *, char *, int))channels_funcs[35])
#define u_addinvite ((int (*)(struct chanset_t *, char *, char *, char *, time_t, int))channels_funcs[36])
#endif
#define tcl_channel_add ((int (*)(Tcl_Interp *, char *, char *))channels_funcs[37])
#define tcl_channel_modify ((int (*)(Tcl_Interp *, struct chanset_t *, int, char **))channels_funcs[38])
#ifdef S_IRCNET
#define write_exempts ((int (*)(FILE *, int))channels_funcs[39])
#define write_invites ((int (*)(FILE *, int))channels_funcs[40])
#endif
#define ismodeline ((int(*)(masklist *, char *))channels_funcs[41])
#define initudef ((void(*)(int, char *,int))channels_funcs[42])
#define ngetudef ((int(*)(char *, char *))channels_funcs[43])
#define expired_mask ((int (*)(struct chanset_t *, char *))channels_funcs[44])
#define remove_channel ((void (*)(struct chanset_t *))channels_funcs[45])
#define global_ban_time (*(int *)(channels_funcs[46]))
#ifdef S_IRCNET
#define global_exempt_time (*(int *)(channels_funcs[47]))
#define global_invite_time (*(int *)(channels_funcs[48]))
#endif
#define write_chans ((int (*)(FILE *, int))channels_funcs[49])
#define write_config ((int (*)(FILE *, int))channels_funcs[50])
#endif
#define isbanned(chan, user) ismasked((chan)->channel.ban, user)
#ifdef S_IRCNET
#define isexempted(chan, user) ismasked((chan)->channel.exempt, user)
#define isinvited(chan, user) ismasked((chan)->channel.invite, user)
#endif
#define ischanban(chan, user) ismodeline((chan)->channel.ban, user)
#ifdef S_IRCNET
#define ischanexempt(chan, user) ismodeline((chan)->channel.exempt, user)
#define ischaninvite(chan, user) ismodeline((chan)->channel.invite, user)
#endif
#define u_setsticky_ban(chan, host, sticky) u_setsticky_mask(chan, ((struct chanset_t *)chan) ? ((struct chanset_t *)chan)->bans : global_bans, host, sticky, "s")
#ifdef S_IRCNET
#define u_setsticky_exempt(chan, host, sticky) u_setsticky_mask(chan, ((struct chanset_t *)chan) ? ((struct chanset_t *)chan)->exempts : global_exempts, host, sticky, "se")
#define u_setsticky_invite(chan, host, sticky) u_setsticky_mask(chan, ((struct chanset_t *)chan) ? ((struct chanset_t *)chan)->invites : global_invites, host, sticky, "sInv")
#endif
#endif
