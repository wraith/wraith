/*
 * proto.h
 *   prototypes for every function used outside its own module
 *
 * (i guess i'm not very modular, cuz there are a LOT of these.)
 * with full prototyping, some have been moved to other .h files
 * because they use structures in those
 * (saves including those .h files EVERY time) - Beldin
 *
 */

#ifndef _EGG_PROTO_H
#define _EGG_PROTO_H

#include "lush.h"
#include "misc_file.h"

#ifdef HAVE_DPRINTF
#define dprintf dprintf_eggdrop
#endif

#define STR(x) x

struct chanset_t;		/* keeps the compiler warnings down :) */
struct userrec;
struct maskrec;
struct igrec;
struct flag_record;
struct list_type;
struct tand_t_struct;

#if !defined(MAKING_MODS)
extern void (*encrypt_pass) (char *, char *);
extern char *(*encrypt_string) (char *, char *);
extern char *(*decrypt_string) (char *, char *);

//extern int lfprintf(FILE *, char *, ...);

extern int (*rfc_casecmp) (const char *, const char *);
extern int (*rfc_ncasecmp) (const char *, const char *, int);
extern int (*rfc_toupper) (int);
extern int (*rfc_tolower) (int);
extern int (*match_noterej) (struct userrec *, char *);
#endif

/* botcmd.c */
void bounce_simul(int, char *);
void send_remote_simul(int, char *, char *, char *);
void bot_share(int, char *);
void bot_shareupdate(int, char *);
int base64_to_int(char *);


/* pcrypt.c */
char *cryptit (char *);
char *decryptit (char *);
int lfprintf(FILE *, char *, ...);

/* botnet.c */
void lower_bot_linked(int idx);
void higher_bot_linked(int idx);
void answer_local_whom(int, int);
char *lastbot(char *);
int nextbot(char *);
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
#ifdef HUB
void botnet_send_limitcheck(struct chanset_t * chan);
#endif
#ifdef S_DCCPASS
void botnet_send_cmdpass(int, char *, char *);
#endif
void zapfbot(int);
void tandem_relay(int, char *, int);
int getparty(char *, int);

/* botmsg.c */
void botnet_send_cfg(int idx, struct cfg_entry *entry);
void botnet_send_cfg_broad(int idx, struct cfg_entry *entry);
void putbot(char *, char *);
void putallbots(char *);
int add_note(char *, char *, char *, int, int);
int simple_sprintf EGG_VARARGS(char *, arg1);
void tandout_but EGG_VARARGS(int, arg1);
char *int_to_base10(int);
char *unsigned_int_to_base10(unsigned int);
char *int_to_base64(unsigned int);
#ifdef S_DCCPASS
void botnet_send_cmdpass(int, char *, char *);
#endif

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
void rmspace(char *s);
void check_timers();
void set_chanlist(const char *host, struct userrec *rec);
void clear_chanlist(void);
void clear_chanlist_member(const char *nick);

/* cmds.c */
int check_dcc_attrs(struct userrec *, int);
int check_dcc_chanattrs(struct userrec *, char *, int, int);
int stripmodes(char *);
char *stripmasktype(int);
void gotremotecmd(char * forbot, char * frombot, char * fromhand, char * fromidx, char * cmd);
void gotremotereply(char * frombot, char * tohand, char * toidx, char * ln);

/* dcc.c */
void failed_link(int);
void dupwait_notify(char *);
char *rand_dccresp();

/* dccutil.c */
void dprintf EGG_VARARGS(int, arg1);
void chatout EGG_VARARGS(char *, arg1);
extern void (*shareout) ();
extern void (*sharein) (int, char *);
extern void (*shareupdatein) (int, char *);
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
void changeover_dcc(int, struct dcc_table *, int);

/* dns.c */
extern void (*dns_hostbyip) (IP);
extern void (*dns_ipbyhost) (char *);
void block_dns_hostbyip(IP);
void block_dns_ipbyhost(char *);
void call_hostbyip(IP, char *, int);
void call_ipbyhost(char *, IP, int);
void dcc_dnshostbyip(IP);
void dcc_dnsipbyhost(char *);

/* gotdcc.c */
void gotdcc(char *, char *, struct userrec *, char *);
void do_boot(int, char *, char *);
int detect_dcc_flood(time_t *, struct chat_info *, int);

/* main.c */
void do_fork();
int crontab_exists();
void crontab_create(int);
void fatal(const char *, int);
int expected_memory(void);
void eggContext(const char *, int, const char *);
void eggContextNote(const char *, int, const char *, const char *);
void eggAssert(const char *, int, const char *);
void backup_userfile(void);


/* match.c */
int _wild_match(register unsigned char *, register unsigned char *);
int _wild_match_per(register unsigned char *, register unsigned char *);

#define wild_match(a,b) _wild_match((unsigned char *)(a),(unsigned char *)(b))
#define wild_match_per(a,b) _wild_match_per((unsigned char *)(a),(unsigned char *)(b))

/* mem.c */
void *n_malloc(int, const char *, int);
void *n_realloc(void *, int, const char *, int);
void n_free(void *, const char *, int);
void tell_mem_status(char *);
void tell_mem_status_dcc(int);
void debug_mem_to_dcc(int);

/* settings.c */
char *progname();
void init_settings();

/* misc.c */
void werr(int);
char *werr_tostr(int);
void sdprintf EGG_VARARGS(char *, arg1);
int listen_all(int, int);
char *getfullbinname(char *);
char *replace(char *, char *, char *);
#ifdef S_GARBLESTRINGS
char *degarble(int, char *);
#endif
void detected(int, char *);
int new_auth();
int isauthed(char *);
void removeauth(int);
char *makehash(struct userrec *, char *);
int goodpass(char *, int, char *);
void check_last();
void check_promisc();
void check_trace(int);
void check_processes();
void makeplaincookie(char *, char *, char *);
int isupdatehub();
int getting_users();
char *kickreason(int);
int bot_aggressive_to(struct userrec *);
void set_cfg_int(char *target, char *entryname, int data);
void set_cfg_str(char *target, char *entryname, char *data);
void add_cfg(struct cfg_entry *entry);
void got_config_share(int idx, char *ln);
void userfile_cfg_line(char *ln);
void trigger_cfg_changed();
void EncryptFile(char *, char *);
void DecryptFile(char *, char *);
int updatebin(int, char *, int);
void got_config_share (int idx, char * ln);
int shell_exec(char * cmdline, char * input, char ** output, char ** erroutput);
int prand(int *seed, int range);
int egg_strcatn(char *dst, const char *src, size_t max);
int my_strcpy(char *, char *);
void putlog EGG_VARARGS(int, arg1);
int ischanhub();
void maskhost(const char *, char *);
char *stristr(char *, char *);
void splitc(char *, char *, char);
void splitcn(char *, char *, char, size_t);
char *newsplit(char **);
char *splitnick(char **);
void stridx(char *, char *, int);
void dumplots(int, const char *, char *);
void daysago(time_t, time_t, char *);
void days(time_t, time_t, char *);
void daysdur(time_t, time_t, char *);
void show_motd(int);
void show_channels(int, char *);
void show_banner(int);
char *extracthostname(char *);
void make_rand_str(char *, int);
int oatoi(const char *);
char *str_escape(const char *str, const char div, const char mask);
char *strchr_unescape(char *str, const char div, register const char esc_char);
void str_unescape(char *str, register const char esc_char);
void kill_bot(char *, char *);
int strcasecmp2(char *, char *);
#ifdef S_DCCPASS
int check_cmd_pass(char *,char *);
int has_cmd_pass(char *);
void set_cmd_pass(char *, int);
#endif


/* net.c */
#ifdef HAVE_SSL
int ssl_cleanup();
int ssl_link(int, int);
#endif /* HAVE_SSL */
IP my_atoul(char *);
unsigned long iptolong(IP);
char *myipstr(int);
IP getmyip();
void cache_my_ip();
void neterror(char *);
void setsock(int, int);
int allocsock(int, int);
#ifdef USE_IPV6
int getsock(int, int);
#else
int getsock(int);
#endif /* USE_IPV6 */
int sockprotocol(int);
int hostprotocol(char *);
char *hostnamefromip(unsigned long);
void dropssl(int);
void real_killsock(int, const char *, int);
int answer(int, char *, unsigned long *, unsigned short *, int);
inline int open_listen(int *);
inline int open_listen_by_af(int *, int);
#ifdef USE_IPV6
int open_address_listen(IP addr, int af_def, int *);
#else
int open_address_listen(IP addr, int *);
#endif /* USE_IPV6 */
int open_telnet(char *, int);
int open_telnet_dcc(int, char *, char *);
int open_telnet_raw(int, char *, int);
void tputs(int, char *, unsigned int);
void dequeue_sockets();
int sockgets(char *, int *);
void tell_netdebug(int);
int sanitycheck_dcc(char *, char *, char *, char *);
void send_timesync(int);
int hostsanitycheck_dcc(char *, char *, IP, char *, char *);
char *iptostr(IP);
int sock_has_data(int, int);
int sockoptions(int sock, int operation, int sock_options);
int flush_inbuf(int idx);

/* tcl.c */
void protect_tcl();
void unprotect_tcl();
void do_tcl(char *, char *);
int readtclprog(char *fname);
int findidx(int);
int findanyidx(int);

/* userent.c */
void list_type_kill(struct list_type *);
int list_type_expmem(struct list_type *);
int xtra_set();
void stats_add(struct userrec *, int, int);


/* userrec.c */
void deflag_user(struct userrec *, int, char *, struct chanset_t *);
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
int write_user(struct userrec *u, FILE * f, int shr);
void write_userfile(int);
struct userrec *check_dcclist_hand(char *);
void touch_laston(struct userrec *, char *, time_t);
void user_del_chan(char *);
char *fixfrom(char *);

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
int readuserfile(char *, struct userrec **);
void check_pmode();
void link_pref_val(struct userrec *u, char *lval);

/* rfc1459.c */
int _rfc_casecmp(const char *, const char *);
int _rfc_ncasecmp(const char *, const char *, int);
int _rfc_toupper(int);
int _rfc_tolower(int);

/* sort.h */
void strsort(char **, unsigned);

#endif				/* _EGG_PROTO_H */
