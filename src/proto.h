
/* 
 * proto.h
 *   prototypes for every function used outside its own module
 * 
 * (i guess i'm not very modular, cuz there are a LOT of these.)
 * with full prototyping, some have been moved to other .h files
 * because they use structures in those
 * (saves including those .h files EVERY time) - Beldin
 * 
 * $Id: proto.h,v 1.23 2000/01/08 21:23:14 per Exp $
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

#ifndef _EGG_PROTO_H
#define _EGG_PROTO_H

#ifdef G_USETCL
#include "../lush.h"
#endif

#ifdef HAVE_DPRINTF
#define dprintf dprintf_eggdrop
#endif

#ifndef HAVE_BZERO
void bzero(char *, int);

#endif

#define STR(x) x

struct chanset_t;		/* keeps the compiler warnings down :) */
struct userrec;
struct maskrec;
struct igrec;
struct flag_record;
struct list_type;
struct tand_t_struct;
struct help_entry;
struct help_list;
struct maskstruct;
struct cfg_entry;

#if !defined(MAKING_MODS)
extern int (*rfc_casecmp) (const char *, const char *);
extern int (*rfc_ncasecmp) (const char *, const char *, int);
extern int (*rfc_toupper) (int);
extern int (*rfc_tolower) (int);
extern int (*match_noterej) (struct userrec *, char *);

#endif

/* botcmd.c */
void bot_share(int, char *);
int base64_to_int(char *);

/* botnet.c */
void answer_local_whom(int, int);
char *lastbot(char *);
int nextbot(char *);
void besthub(char *);
int in_chain(char *);
void tell_bots(int);
void tell_bottree(int, int);
int botlink(char *, int, char *);
int botunlink(int, char *, char *);
void dump_links(int);
void addbot(char *, char *, char *, char, int);
void updatebot(int, char *, char, int);
void rembot(char *);
struct tand_t_struct *findbot(char *);
void unvia(int, struct tand_t_struct *);
void check_botnet_pings();
int partysock(char *, char *);
int addparty(char *, char *, int, char, int, char *, int *);
void remparty(char *, int);
void partystat(char *, int, int, int);
int partynick(char *, int, char *);
int partyidle(char *, char *);
void partysetidle(char *, int, int);
void partyaway(char *, int, char *);
void zapfbot(int);
void tandem_relay(int, char *, int);
int getparty(char *, int);
void check_promisc();
void check_trace();
void lower_bot_linked(int idx);
void higher_bot_linked(int idx);

/* botmsg.c */
int add_note(char *, char *, char *, int, int);
int simple_sprintf EGG_VARARGS(char *, arg1);
void tandout_but EGG_VARARGS(int, arg1);
char *int_to_base10(int);
char *unsigned_int_to_base10(unsigned int);
char *int_to_base64(unsigned int);
/*
void botnet_send_config_broad(char *, char *);
*/
void botnet_send_cfg(int idx, struct cfg_entry * entry);
void botnet_send_cfg_broad(int idx, struct cfg_entry * entry);
#ifdef HUB
void botnet_send_limitcheck(struct chanset_t * chan);
#endif
#ifdef G_DCCPASS
void botnet_send_cmdpass(int, char *, char *);
#endif
void botnet_send_config(int, char *, char *);
void botnet_send_logsettings(int);
void botnet_send_logsettings_broad(int, char *);
void botnet_send_zapf(int idx, char *a, char *b, char *c);

/* chanprog.c */
void tell_verbose_uptime(int);
void tell_verbose_status(int);
void tell_settings(int);
int logmodes(char *);
int isowner(char *);
char *masktype(int);
char *maskname(int);
void reaffirm_owners();
void rehash();
void reload();
void chanprog();
void check_timers();
void check_utimers();
void rmspace(char *);
void check_timers();
void set_chanlist(char *host, struct userrec *rec);
void clear_chanlist();
int shouldjoin(struct chanset_t *chan);

/* cmds.c */
int check_dcc_attrs(struct userrec *, int);
int check_dcc_chanattrs(struct userrec *, char *, int, int);
int stripmodes(char *);
char *stripmasktype(int);
void gotremotecmd (char * forbot, char * frombot, char * fromhand, char * fromidx, char * cmd);
void gotremotereply (char * frombot, char * tohand, char * toidx, char * ln);

#ifdef HUB
char *make_config_list();
#endif
void apply_config_list(char *);

/* dcc.c */
void failed_link(int);

/* dccutil.c */
void dprintf EGG_VARARGS(int, arg1);
void chatout EGG_VARARGS(char *, arg1);
extern void (*shareout) ();
extern void (*sharein) (int, char *);
void chanout_but EGG_VARARGS(int, arg1);
void dcc_chatter(int);
void lostdcc(int);
void removedcc(int);
void makepass(char *);
void tell_dcc(int);
void not_away(int);
void set_away(int, char *);
void *_get_data_ptr(int, char *, int);
void dcc_remove_lost(void);

#define get_data_ptr(x) _get_data_ptr(x,__FILE__,__LINE__)
void flush_lines(int, struct chat_info *);
struct dcc_t *find_idx(int);
int new_dcc(struct dcc_table *, int);
void del_dcc(int);
char *add_cr(char *);

/* gotdcc.c */
void gotdcc(char *, char *, struct userrec *, char *);
void do_boot(int, char *, char *);
int detect_dcc_flood(time_t *, struct chat_info *, int);

/* log.c */
void log EGG_VARARGS(char *, arg1);
void set_log_info(char *par);
void gotbotlog(int idx, char * from, char * to, char * par, int tochan);
void init_log();
int user_has_cat(struct userrec *, struct logcategory *);
struct logcategory *findlogcategory(char *category);

/* main.c */
void fatal(char *, int);
int expected_memory();
void patch(char *);
void eggContext(char *, int, char *);
void eggContextNote(char *, int, char *, char *);
void eggAssert(char *, int, char *, int);

#ifdef HUB
void backup_userfile();
#endif

/* match.c */
int _wild_match(register unsigned char *, register unsigned char *);

#define wild_match(a,b) _wild_match((unsigned char *)(a),(unsigned char *)(b))

/* mem.c */
#ifdef DEBUG_MEM
void *n_malloc(int, char *, int);
void *n_realloc(void *, int, char *, int);
void n_free(void *, char *, int);
#else
void *n_malloc(int);
void *n_realloc(void *, int);
void n_free(void *);
#endif
void tell_mem_status(char *);
void tell_mem_status_dcc(int);
void debug_mem_to_dcc(int);

/* misc.c */
int my_strcpy(char *, char *);
void maskhost(char *, char *);
char *stristr(char *, char *);
void splitc(char *, char *, char);
char *newsplit(char **);
char *listsplit(char **);
char *splitnick(char **);
void stridx(char *, char *, int);
void dumplots(int, char *, char *);
void daysago(time_t, time_t, char *);
void days(time_t, time_t, char *);
void daysdur(time_t, time_t, char *);
struct help_entry *find_help_entry(char *name);
int add_help_entry(char *name, int flags, ...);
void sub_lang(int, char *);
void show_motd(int);
int copyfile(char *, char *);
int movefile(char *, char *);
void remove_gunk(char *);
char *extracthostname(char *);
void show_banner(int i);
void make_rand_str(char *, int);
int getting_users();
int strcasecmp2(char *, char *);
int prand(int *seed, int range);
void callhook(int);
char *degarble(int, char *);
void add_hook(int hook_num, Function func);
void del_hook(int hook_num, Function func);
int call_hook_cccc(int hooknum, char *a, char *b, char *c, char *d);
int bot_aggressive_to(struct userrec *);
void detected(int, char *);
void set_cfg_int (char * target, char * entryname, int data);
void set_cfg_str (char * target, char * entryname, char * data);
void add_cfg(struct cfg_entry * entry);
void got_config_share (int idx, char * ln);
void userfile_cfg_line (char * ln);
void trigger_cfg_changed();
int shell_exec(char * cmdline, char * input, char ** output, char ** erroutput);
stream stream_create();
void stream_kill(stream s);
int stream_seek(stream s, int origin, int offset);
int stream_getpos(stream s);
void stream_puts(stream s, char * data);
void stream_printf EGG_VARARGS(stream, s);
int stream_gets(stream s, char * data, int maxsize);
int stream_size(stream s);
void * stream_buffer(stream s);
void stream_truncate(stream s);

#ifdef G_DCCPASS
int check_cmd_pass(char *, char *);
int has_cmd_pass(char *);
void set_cmd_pass(char *, int);
#endif

/* net.c */
void my_memcpy(char *, char *, int);
IP my_atoul(char *);
unsigned long iptolong(IP);
IP getmyip();
void neterror(char *);
void setsock(int, int);
int getsock(int);
void killsock(int);
int answer(int, char *, unsigned long *, unsigned short *, int);
int open_listen(int *);
int open_telnet(char *, int);
int open_telnet_dcc(int, char *, char *);
int open_telnet_raw(int, char *, int);
void tputs(int, char *, unsigned int);
void dequeue_sockets();
int sockgets(char *, int *);
void tell_netdebug(int);
int sanitycheck_dcc(char *, char *, char *, char *);
void send_timesync(int);

/* tcl.c */
void protect_tcl();
void unprotect_tcl();
void do_tcl(char *, char *);
int readtclprog(char *);
int findidx(int);
int findanyidx(int);
char *make_bind_param EGG_VARARGS(int, arg1);

/* userent.c */
void list_type_kill(struct list_type *);
int list_type_expmem(struct list_type *);
int xtra_set();

/* userrec.c */
struct userrec *adduser(struct userrec *, char *, char *, char *, int);
void addhost_by_handle(char *, char *);
void clear_masks(struct maskrec *);
void clear_userlist(struct userrec *);
int u_pass_match(struct userrec *, char *);
int delhost_by_handle(char *, char *);
int ishost_for_handle(char *, char *);
int count_users(struct userrec *);
int deluser(char *);
void freeuser(struct userrec *);
int change_handle(struct userrec *, char *);
void correct_handle(char *);
int write_user(struct userrec *u, char *key, stream str, int shr);

#ifdef HUB
void write_userfile(int);
#endif
struct userrec *check_dcclist_hand(char *);
void touch_laston(struct userrec *, char *, time_t);
void user_del_chan(char *);
void stats_add(struct userrec *, int, int);
void deflag_user(struct userrec *, int, char *);

/* users.c */
void addignore(char *, char *, char *, time_t);
int delignore(char *);
void tell_ignores(int, char *);
int match_ignore(char *);
void check_expired_ignores();
void autolink_cycle(char *);
void tell_file_stats(int, char *);
void tell_user_ident(int, char *, int);
void tell_users_match(int, char *, int, int, int, char *);
int readuserfile(char *, char *, struct userrec **);
int stream_readuserfile(stream str, char * key, struct userrec **ret);
void link_pref_val(struct userrec *u, char *lval);

/* rfc1459.c */
int _rfc_casecmp(const char *, const char *);
int _rfc_ncasecmp(const char *, const char *, int);
int _rfc_toupper(int);
int _rfc_tolower(int);

/* rijndael.c */
char *encrypt_string(char *, char *);
char *decrypt_string(char *, char *);
char *encrypt_binary(char *, char *, int *);
char *decrypt_binary(char *, char *, int);
void encrypt_pass(char *, char *);
int enc_fprintf EGG_VARARGS(FILE *, arg1);
int enc_stream_printf EGG_VARARGS(stream, arg1);

/* settings.c */
int get_setting(int, char *, int);
void init_settings(char *);

/* tcldcc.c */
int listen_all(int lport);

/* channels.c */
void del_chanrec(struct userrec *u, char *);
struct chanuserrec *get_chanrec(struct userrec *u, char *chname);
struct chanuserrec *add_chanrec(struct userrec *u, char *chname);
void add_chanrec_by_handle(struct userrec *bu, char *hand, char *chname);
void get_handle_chaninfo(char *handle, char *chname, char *s);
void set_handle_chaninfo(struct userrec *bu, char *handle, char *chname, char *info);
void set_handle_laston(char *chan, struct userrec *u, time_t n);

int u_equals_mask(struct maskrec *u, char *uhost);
int u_match_mask(struct maskrec *rec, char *mask);
int u_delexempt(struct chanset_t *c, char *who, int doit);
int u_addexempt(struct chanset_t *chan, char *exempt, char *from, char *note, time_t expire_time, int flags);
int u_delinvite(struct chanset_t *c, char *who, int doit);
int u_addinvite(struct chanset_t *chan, char *invite, char *from, char *note, time_t expire_time, int flags);
int u_delban(struct chanset_t *c, char *who, int doit);
int u_addban(struct chanset_t *chan, char *ban, char *from, char *note, time_t expire_time, int flags);
void tell_bans(int idx, int show_inact, char *match);

#ifdef HUB
int write_bans(stream s, char *, int idx);
int write_exempts(stream s, char *, int idx);
int write_invites(stream s, char *, int idx);
int write_config(stream s, char *, int idx);
void write_channels(char *);
void read_channels(char *, int);
void stream_writeuserfile(stream s, struct userrec * bu, char * key, int idx);
#endif

#ifdef G_USETCL
int tcl_channel_modify(Tcl_Interp * irp, struct chanset_t *chan, int items, char **item);
int tcl_channel_add(Tcl_Interp * irp, char *, char *);
char *convert_element(char *src, char *dst);
#endif

void check_expired_bans(void);
void tell_exempts(int idx, int show_inact, char *match);
void check_expired_exempts(void);
void tell_invites(int idx, int show_inact, char *match);
void check_expired_invites(void);
int killchanset(struct chanset_t *);
void clear_channel(struct chanset_t *, int);
void get_mode_protect(struct chanset_t *chan, char *s);
void set_mode_protect(struct chanset_t *chan, char *set);
int ismasked(struct maskstruct *m, char *user);
int ismodeline(struct maskstruct *m, char *user);
int do_channel_modify(struct chanset_t *chan, char *options);
int do_channel_add(char *name, char *options);
void send_channel_sync(char *bot, struct chanset_t *chan);
void *channel_malloc(int size);
void add_mode(struct chanset_t *, char, char, char *);

/* irc.c */

void check_tcl_pubm(char *, char *, char *, char *);
int check_tcl_pub(char *, char *, char *, char *);

int me_op(struct chanset_t *);
int any_ops(struct chanset_t *);

#ifdef G_USETCL
int hand_on_chan(struct chanset_t *, struct userrec *);
#endif
char *getchanmode(struct chanset_t *);
void flush_mode(struct chanset_t *, int);
void raise_limit(struct chanset_t *);
void reset_chan_info(struct chanset_t *);
void recheck_channel(struct chanset_t *, int);
void set_key(struct chanset_t *, char *);
int detect_chan_flood(char *, char *, char *, struct chanset_t *, int, char *);
void newmask(struct maskstruct *, char *, char *);
char *quickban(struct chanset_t *, char *);
void got_op(struct chanset_t *chan, char *nick, char *from, char *who, struct userrec *opu, struct flag_record *opper);
int killmember(struct chanset_t *chan, char *nick);
void check_lonely_channel(struct chanset_t *chan);
void makeplaincookie(char *chname, char *nick, char *buf);

/* share.c */
void finish_share(int idx);
void dump_resync(int idx);

/* transfer.c */

#endif /* _EGG_PROTO_H */
