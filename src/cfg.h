#ifndef _CFG_H
#define _CFG_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define CFGF_GLOBAL  1          /* Accessible as .config */
#define CFGF_LOCAL   2          /* Accessible as .botconfig */

typedef struct cfg_entry {
  char *name;
  int flags;
  char *gdata;
  char *ldata;
  void (*globalchanged) (struct cfg_entry *, char *oldval, int *valid);
  void (*localchanged) (struct cfg_entry *, char *oldval, int *valid);
  void (*describe) (struct cfg_entry *, int idx);
} cfg_entry_T;

extern struct cfg_entry CFG_MOTD, CFG_CMDPREFIX, CFG_BADCOOKIE, CFG_MANUALOP, CFG_MDOP, CFG_MOP, CFG_FORKINTERVAL;
#if defined(S_AUTHHASH) || defined(S_DCCAUTH)
extern struct cfg_entry CFG_AUTHKEY;
#endif /* S_AUTHHASH || S_DCCAUTH */
#ifdef S_MSGCMDS
extern struct cfg_entry CFG_MSGOP, CFG_MSGPASS, CFG_MSGINVITE, CFG_MSGIDENT;
#endif /* S_MSGCMDS */
#ifdef G_MEAN
extern struct cfg_entry CFG_MEANDEOP, CFG_MEANKICK, CFG_MEANBAN;
#endif /* G_MEAN */
#ifdef S_LASTCHECK
extern struct cfg_entry CFG_LOGIN;
#endif /* S_LASTCHECK */
#ifdef S_HIJACKCHECK
extern struct cfg_entry CFG_HIJACK;
#endif /* S_HIJACKCHECK */
#ifdef S_ANTITRACE
extern struct cfg_entry CFG_TRACE;
#endif /* S_ANTITRACE */
#ifdef S_PROMISC
extern struct cfg_entry CFG_PROMISC;
#endif /* S_PROMISC */
#ifdef S_PROCESSCHECK
extern struct cfg_entry CFG_BADPROCESS, CFG_PROCESSLIST;
#endif /* S_PROCESSCHECK */

#ifdef HUB
extern struct cfg_entry CFG_SERVERS, CFG_SERVERS6, CFG_NICK, CFG_REALNAME,
	CFG_INBOTS, CFG_LAGTHRESHOLD, CFG_OPREQUESTS, CFG_OPTIMESLACK;
#ifdef S_AUTOLOCK
struct cfg_entry CFG_FIGHTTHRESHOLD;
#endif /* S_AUTOLOCK */
#endif /* HUB */

void set_cfg_str(char *, char *, char *);
void add_cfg(struct cfg_entry *);
void got_config_share(int, char *, int);
void userfile_cfg_line(char *);
void trigger_cfg_changed();
#ifdef S_DCCPASS
int check_cmd_pass(const char *, char *);
int has_cmd_pass(const char *);
void set_cmd_pass(char *, int);

extern struct cmd_pass            *cmdpass;
#endif /* S_DCCPASS */

extern char			cmdprefix[];
extern int			cfg_count, cfg_noshare;
extern struct cfg_entry		**cfg;

#endif /* !_CFG_H */
