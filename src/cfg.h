#ifndef _CFG_H
#define _CFG_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define CFGF_GLOBAL  BIT0          /* Accessible as .config */
#define CFGF_LOCAL   BIT1          /* Accessible as .botconfig */
#define CFGF_HUB     BIT2
#define CFGF_LEAF    BIT3

typedef struct cfg_entry {
  char *name;
  int flags;
  char *gdata;
  char *ldata;
  void (*globalchanged) (struct cfg_entry *, int *valid);
  void (*localchanged) (struct cfg_entry *, int *valid);
  void (*describe) (struct cfg_entry *, int idx);
} cfg_entry_T;

extern struct cfg_entry CFG_MOTD, CFG_CMDPREFIX, CFG_FORKINTERVAL, CFG_CHANSET, CFG_SERVERS, CFG_SERVERS6, 
                        CFG_NICK, CFG_REALNAME, CFG_INBOTS, CFG_LAGTHRESHOLD, CFG_OPREQUESTS,
                        CFG_OPBOTS, CFG_INBOTS, CFG_SERVPORT, CFG_AUTHKEY, CFG_MSGOP, CFG_MSGPASS, 
			CFG_MSGINVITE, CFG_MSGIDENT, CFG_LOGIN, CFG_HIJACK, CFG_TRACE, CFG_PROMISC, 
			CFG_BADPROCESS, CFG_PROCESSLIST, CFG_FIGHTTHRESHOLD, CFG_CLOSETHRESHOLD, CFG_KILLTHRESHOLD,
			CFG_DCCAUTH;
#ifdef G_MEAN
extern struct cfg_entry CFG_MEANDEOP, CFG_MEANKICK, CFG_MEANBAN;
#endif /* G_MEAN */
void deflag_user(struct userrec *, int, char *, struct chanset_t *);
int deflag_translate(const char *);
void set_cfg_str(char *, char *, char *);
void add_cfg(struct cfg_entry *);
void got_config_share(int, char *, int);
void userfile_cfg_line(char *);
void trigger_cfg_changed();
int check_cmd_pass(const char *, char *);
int has_cmd_pass(const char *);
void set_cmd_pass(char *, int);
void init_cfg(void);

extern struct cmd_pass            *cmdpass;

extern char			cmdprefix;
extern int			cfg_count;
extern bool			cfg_noshare;
extern struct cfg_entry		**cfg;

#define OP_BOTS (CFG_OPBOTS.gdata ? atoi(CFG_OPBOTS.gdata) : 1)
#define IN_BOTS (CFG_INBOTS.gdata ? atoi(CFG_INBOTS.gdata) : 1)
#define LAG_THRESHOLD (CFG_LAGTHRESHOLD.gdata ? atoi(CFG_LAGTHRESHOLD.gdata) : 15)
#define OPREQ_COUNT (CFG_OPREQUESTS.gdata ? atoi( CFG_OPREQUESTS.gdata ) : 2)
#define OPREQ_SECONDS (CFG_OPREQUESTS.gdata ? atoi( strchr(CFG_OPREQUESTS.gdata, ':') + 1 ) : 5)
#define msgop (CFG_MSGOP.ldata ? CFG_MSGOP.ldata : CFG_MSGOP.gdata ? CFG_MSGOP.gdata : "")
#define msgpass (CFG_MSGPASS.ldata ? CFG_MSGPASS.ldata : CFG_MSGPASS.gdata ? CFG_MSGPASS.gdata : "")
#define msginvite (CFG_MSGINVITE.ldata ? CFG_MSGINVITE.ldata : CFG_MSGINVITE.gdata ? CFG_MSGINVITE.gdata : "")
#define msgident (CFG_MSGIDENT.ldata ? CFG_MSGIDENT.ldata : CFG_MSGIDENT.gdata ? CFG_MSGIDENT.gdata : "")
#define kill_threshold (CFG_KILLTHRESHOLD.gdata ? atoi(CFG_KILLTHRESHOLD.gdata) : 0)
#define dccauth (CFG_DCCAUTH.ldata ? atoi(CFG_DCCAUTH.ldata) : CFG_DCCAUTH.gdata ? atoi(CFG_DCCAUTH.gdata) : 0)

#endif /* !_CFG_H */
