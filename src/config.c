/*
 * config.c -- handles:
 *   botnet config
 *   cmdpass functions
 */

#include "main.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include "chan.h"
#include "tandem.h"
#include "modules.h"
#include <net/if.h>

#include "stat.h"
#include "bg.h"

extern char			botnetnick[];
extern struct userrec 		*userlist;
extern time_t		 	now;

int 				cfg_count = 0, cfg_noshare = 0;
struct cfg_entry 		**cfg = NULL;

#ifdef S_AUTH
char 				authkey[121];		/* This is one of the keys used in the auth hash */
#endif /* S_AUTH */

char 				cmdprefix[1] = "+";	/* This is the prefix for msg/channel cmds */

struct cfg_entry CFG_MOTD = {
  "motd", CFGF_GLOBAL, NULL, NULL,
  NULL, NULL, NULL
};

#ifdef S_AUTH
void authkey_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("authkey is used for authing, give to your users if they are to use DCC chat or IRC cmds. (can be bot specific)\n"));
}

void authkey_changed(struct cfg_entry * entry, char * olddata, int * valid) {
  if (entry->ldata) {
    strncpyz(authkey, (char *) entry->ldata, sizeof authkey);
  } else if (entry->gdata) {
    strncpyz(authkey, (char *) entry->gdata, sizeof authkey);
  }
}

struct cfg_entry CFG_AUTHKEY = {
  "authkey", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  authkey_changed, authkey_changed, authkey_describe
};
#endif /* S_AUTH */

#ifdef S_MSGCMDS
struct cfg_entry CFG_MSGOP, CFG_MSGPASS, CFG_MSGINVITE;
void msgcmds_describe(struct cfg_entry *entry, int idx) {
  if (entry == &CFG_MSGOP)
    dprintf(idx, STR("msgop defines the cmd for opping via msging the bot (leave blank to disable)\n"));
  else if (entry == &CFG_MSGPASS)
    dprintf(idx, STR("msgpass defines the cmd for setting a pass via msging the bot (leave blank to disable)\n"));
  else if (entry == &CFG_MSGINVITE)
    dprintf(idx, STR("msginvite defines the cmd for requesting invite via msging the bot (leave blank to disable)\n"));
}

void msgcmds_changed(struct cfg_entry * entry, char * olddata, int * valid) {
}

struct cfg_entry CFG_MSGOP = {
  "msgop", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  msgcmds_changed, msgcmds_changed, msgcmds_describe
};

struct cfg_entry CFG_MSGPASS = {
  "msgpass", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  msgcmds_changed, msgcmds_changed, msgcmds_describe
};

struct cfg_entry CFG_MSGINVITE = {
  "msginvite", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  msgcmds_changed, msgcmds_changed, msgcmds_describe
};
#endif /* S_MSGCMDS */

void cmdprefix_describe(struct cfg_entry *entry, int idx) {
  dprintf(idx, STR("cmdprefix is the prefix used for msg cmds, ie: !op or .op\n"));
}

void cmdprefix_changed(struct cfg_entry * entry, char * olddata, int * valid) {
  if (entry->ldata) {
    strncpyz(cmdprefix, (char *) entry->ldata, sizeof cmdprefix);
  } else if (entry->gdata) {
    strncpyz(cmdprefix, (char *) entry->gdata, sizeof cmdprefix);
  }
}

struct cfg_entry CFG_CMDPREFIX = {
  "cmdprefix", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  cmdprefix_changed, cmdprefix_changed, cmdprefix_describe
};


struct cfg_entry
  CFG_BADCOOKIE,
  CFG_MANUALOP,
#ifdef G_MEAN
  CFG_MEANDEOP,
  CFG_MEANKICK,
  CFG_MEANBAN,
#endif /* G_MEAN */
  CFG_MDOP,
  CFG_MOP;


void deflag_describe(struct cfg_entry *cfgent, int idx)
{
  if (cfgent == &CFG_BADCOOKIE)
    dprintf(idx, STR("bad-cookie decides what happens to a bot if it does an illegal op (no/incorrect op cookie)\n"));
  else if (cfgent==&CFG_MANUALOP)
    dprintf(idx, STR("manop decides what happens to a user doing a manual op in a -manop channel\n"));
#ifdef G_MEAN
  else if (cfgent==&CFG_MEANDEOP)
    dprintf(idx, STR("mean-deop decides what happens to a user deopping a bot in a +mean channel\n"));
  else if (cfgent==&CFG_MEANKICK)
    dprintf(idx, STR("mean-kick decides what happens to a user kicking a bot in a +mean channel\n"));
  else if (cfgent==&CFG_MEANBAN)
    dprintf(idx, STR("mean-ban decides what happens to a user banning a bot in a +mean channel\n"));
#endif /* G_MEAN */
  else if (cfgent==&CFG_MDOP)
    dprintf(idx, STR("mdop decides what happens to a user doing a mass deop\n"));
  else if (cfgent==&CFG_MOP)
    dprintf(idx, STR("mop decides what happens to a user doing a mass op\n"));
  dprintf(idx, STR("Valid settings are: ignore (No flag changes), deop (set flags to +d), kick (set flags to +dk) or delete (remove from userlist)\n"));
}

void deflag_changed(struct cfg_entry *entry, char *oldval, int *valid) 
{
  char *p = (char *) entry->gdata;
  if (!p)
    return;
  if (strcmp(p, STR("ignore")) && strcmp(p, STR("deop")) && strcmp(p, STR("kick")) && strcmp(p, STR("delete")))
    *valid=0;
}

struct cfg_entry CFG_BADCOOKIE = {
  "bad-cookie", CFGF_GLOBAL, NULL, NULL,
  deflag_changed, NULL, deflag_describe
};

struct cfg_entry CFG_MANUALOP = {
  "manop", CFGF_GLOBAL, NULL, NULL,
  deflag_changed, NULL, deflag_describe
};

#ifdef G_MEAN
struct cfg_entry CFG_MEANDEOP = {
  "mean-deop", CFGF_GLOBAL, NULL, NULL,
  deflag_changed, NULL, deflag_describe
};


struct cfg_entry CFG_MEANKICK = {
  "mean-kick", CFGF_GLOBAL, NULL, NULL,
  deflag_changed, NULL, deflag_describe
};

struct cfg_entry CFG_MEANBAN = {
  "mean-ban", CFGF_GLOBAL, NULL, NULL,
  deflag_changed, NULL, deflag_describe
};
#endif /* G_MEAN */

struct cfg_entry CFG_MDOP = {
  "mdop", CFGF_GLOBAL, NULL, NULL,
  deflag_changed, NULL, deflag_describe
};

struct cfg_entry CFG_MOP = {
  "mop", CFGF_GLOBAL, NULL, NULL,
  deflag_changed, NULL, deflag_describe
};

void deflag_user(struct userrec *u, int why, char *msg, struct chanset_t *chan)
{
  char tmp[256], tmp2[1024];
  struct cfg_entry *ent = NULL;

  struct flag_record fr = {FR_GLOBAL, FR_CHAN, 0, 0, 0};

  if (!u)
    return;

  switch (why) {
  case DEFLAG_BADCOOKIE:
    strcpy(tmp, STR("Bad op cookie"));
    ent = &CFG_BADCOOKIE;
    break;
  case DEFLAG_MANUALOP:
    strcpy(tmp, STR("Manual op in -manop channel"));
    ent = &CFG_MANUALOP;
    break;
#ifdef G_MEAN
  case DEFLAG_MEAN_DEOP:
    strcpy(tmp, STR("Deopped bot in +mean channel"));
    ent = &CFG_MEANDEOP;
    break;
  case DEFLAG_MEAN_KICK:
    strcpy(tmp, STR("Kicked bot in +mean channel"));
    ent = &CFG_MEANDEOP;
    break;
  case DEFLAG_MEAN_BAN:
    strcpy(tmp, STR("Banned bot in +mean channel"));
    ent = &CFG_MEANDEOP;
    break;
#endif /* G_MEAN */
  case DEFLAG_MDOP:
    strcpy(tmp, STR("Mass deop"));
    ent = &CFG_MDOP;
    break;
  case DEFLAG_MOP:
    strcpy(tmp, STR("Mass op"));
    ent = &CFG_MOP;
    break;
  default:
    ent = NULL;
    sprintf(tmp, STR("Reason #%i"), why);
  }
  if (ent && ent->gdata && !strcmp(ent->gdata, STR("deop"))) {
    putlog(LOG_WARN, "*",  STR("Setting %s +d (%s): %s\n"), u->handle, tmp, msg);
    sprintf(tmp2, STR("+d: %s (%s)"), tmp, msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
    get_user_flagrec(u, &fr, chan->dname);
    fr.global = USER_DEOP;
    fr.chan = USER_DEOP;
    set_user_flagrec(u, &fr, chan->dname);
  } else if (ent && ent->gdata && !strcmp(ent->gdata, STR("kick"))) {
    putlog(LOG_WARN, "*",  STR("Setting %s +dk (%s): %s\n"), u->handle, tmp, msg);
    sprintf(tmp2, STR("+dk: %s (%s)"), tmp, msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
    get_user_flagrec(u, &fr, chan->dname);
    fr.global = USER_DEOP | USER_KICK;
    fr.chan = USER_DEOP | USER_KICK;
    set_user_flagrec(u, &fr, chan->dname);
  } else if (ent && ent->gdata && !strcmp(ent->gdata, STR("delete"))) {
    putlog(LOG_WARN, "*",  STR("Deleting %s (%s): %s\n"), u->handle, tmp, msg);
    deluser(u->handle);
  } else {
    putlog(LOG_WARN, "*",  STR("No user flag effects for %s (%s): %s\n"), u->handle, tmp, msg);
    sprintf(tmp2, STR("Warning: %s (%s)"), tmp, msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
  }
}

void misc_describe(struct cfg_entry *cfgent, int idx)
{
  int i = 0;

  if (!strcmp(cfgent->name, STR("fork-interval"))) {
    dprintf(idx, STR("fork-interval is number of seconds in between each fork() call made by the bot, to change process ID and reset cpu usage counters.\n"));
    i = 1;
#ifdef S_LASTCHECK
  } else if (!strcmp(cfgent->name, STR("login"))) {
    dprintf(idx, STR("login sets how to handle someone logging in to the shell\n"));
#endif /* S_LASTCHECK */
#ifdef S_ANTITRACE
  } else if (!strcmp(cfgent->name, STR("trace"))) {
    dprintf(idx, STR("trace sets how to handle someone tracing/debugging the bot\n"));
#endif /* S_ANTITRACE */
#ifdef S_PROMISC
  } else if (!strcmp(cfgent->name, STR("promisc"))) {
    dprintf(idx, STR("promisc sets how to handle when a interface is set to promiscuous mode\n"));
#endif /* S_PROMISC */
#ifdef S_PROCESSCHECK
  } else if (!strcmp(cfgent->name, STR("bad-process"))) {
    dprintf(idx, STR("bad-process sets how to handle when a running process not listed in process-list is detected\n"));
  } else if (!strcmp(cfgent->name, STR("process-list"))) {
    dprintf(idx, STR("process-list is a comma-separated list of \"expected processes\" running on the bots uid\n"));
    i = 1;
#endif /* S_PROCESSCHECK */
#ifdef S_HIJACKCHECK
  } else if (!strcmp(cfgent->name, STR("hijack"))) {
    dprintf(idx, STR("hijack sets how to handle when a commonly used hijack method attempt is detected. (recommended: die)\n"));
#endif /* S_HIJACKCHECK */
  }
  if (!i)
    dprintf(idx, STR("Valid settings are: nocheck, ignore, warn, die, reject, suicide\n"));
}

void fork_lchanged(struct cfg_entry * cfgent, char * oldval, int * valid) {
  if (!cfgent->ldata)
    return;
  if (atoi(cfgent->ldata)<=0)
    *valid=0;
}

void fork_gchanged(struct cfg_entry * cfgent, char * oldval, int * valid) {
  if (!cfgent->gdata)
    return;
  if (atoi(cfgent->gdata)<=0)
    *valid=0;
}

void fork_describe(struct cfg_entry * cfgent, int idx) {
  dprintf(idx, STR("fork-interval is number of seconds in between each fork() call made by the bot, to change process ID and reset cpu usage counters.\n"));
}

struct cfg_entry CFG_FORKINTERVAL = {
  "fork-interval", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  fork_gchanged, fork_lchanged, fork_describe
};

void detect_lchanged(struct cfg_entry * cfgent, char * oldval, int * valid) {
  char * p = cfgent->ldata;
  if (!p)
    *valid=1;
  else if (strcmp(p, STR("ignore")) && strcmp(p, STR("die")) && strcmp(p, STR("reject"))
           && strcmp(p, STR("suicide")) && strcmp(p, STR("nocheck")) && strcmp(p, STR("warn")))
    *valid=0;
}

void detect_gchanged(struct cfg_entry * cfgent, char * oldval, int * valid) {
  char * p = (char *) cfgent->ldata;
  if (!p)
    *valid=1;
  else if (strcmp(p, STR("ignore")) && strcmp(p, STR("die")) && strcmp(p, STR("reject"))
           && strcmp(p, STR("suicide")) && strcmp(p, STR("nocheck")) && strcmp(p, STR("warn")))
    *valid=0;
}

#ifdef S_LASTCHECK
struct cfg_entry CFG_LOGIN = {
  "login", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, misc_describe
};
#endif /* S_LASTCHECK */
#ifdef S_HIJACKCHECK
struct cfg_entry CFG_HIJACK = {
  "hijack", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, misc_describe
};
#endif /* S_HIJACKCHECK */
#ifdef S_ANTITRACE
struct cfg_entry CFG_TRACE = {
  "trace", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, misc_describe
};
#endif /* S_ANTITRACE */
#ifdef S_PROMISC
struct cfg_entry CFG_PROMISC = {
  "promisc", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, misc_describe
};
#endif /* S_PROMISC */
#ifdef S_PROCESSCHECK
struct cfg_entry CFG_BADPROCESS = {
  "bad-process", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, misc_describe
};

struct cfg_entry CFG_PROCESSLIST = {
  "process-list", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  NULL, NULL, misc_describe
};
#endif /* S_PROCESSCHECK */

#ifdef S_DCCPASS
struct cmd_pass *cmdpass = NULL;
#endif /* S_DCCPASS */

/* this is cfg shit from servers/irc/ctcp because hub doesnt load
 * these mods */
#ifdef HUB
void servers_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("servers is a comma-separated list of servers the bot will use\n"));
}
void servers_changed(struct cfg_entry * entry, char * olddata, int * valid) {
}
void servers6_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("servers6 is a comma-separated list of servers the bot will use (FOR IPv6)\n"));
}
void servers6_changed(struct cfg_entry * entry, char * olddata, int * valid) {
}
void nick_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("nick is the bots preferred nick when connecting/using .resetnick\n"));
}
void nick_changed(struct cfg_entry * entry, char * olddata, int * valid) {
}
void realname_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("realname is the bots \"real name\" when connecting\n"));
}

void realname_changed(struct cfg_entry * entry, char * olddata, int * valid) {
}
struct cfg_entry CFG_SERVERS = {
  "servers", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  servers_changed, servers_changed, servers_describe
};
struct cfg_entry CFG_SERVERS6 = {
  "servers6", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  servers6_changed, servers6_changed, servers6_describe
};

struct cfg_entry CFG_NICK = {
  "nick", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  nick_changed, nick_changed, nick_describe
};

struct cfg_entry CFG_REALNAME = {
  "realname", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  realname_changed, realname_changed, realname_describe
};

void getin_describe(struct cfg_entry *cfgent, int idx)
{
  if (!strcmp(cfgent->name, STR("op-bots")))
    dprintf(idx, STR("op-bots is number of bots to ask every time a oprequest is to be made\n"));
  else if (!strcmp(cfgent->name, STR("in-bots")))
    dprintf(idx, STR("in-bots is number of bots to ask every time a inrequest is to be made\n"));
  else if (!strcmp(cfgent->name, STR("op-requests")))
    dprintf(idx, STR("op-requests (requests:seconds) limits how often the bot will ask for ops\n"));
  else if (!strcmp(cfgent->name, STR("lag-threshold")))
    dprintf(idx, STR("lag-threshold is maximum acceptable server lag for the bot to send/honor requests\n"));
  else if (!strcmp(cfgent->name, STR("op-time-slack")))
    dprintf(idx, STR("op-time-slack is number of seconds a opcookies encoded time value can be off from the bots current time\n"));
  else if (!strcmp(cfgent->name, STR("lock-threshold")))
    dprintf(idx, STR("Format H:L. When at least H hubs but L or less leafs are linked, lock all channels\n"));
  else if (!strcmp(cfgent->name, STR("kill-threshold")))
    dprintf(idx, STR("When more than kill-threshold bots have been killed/k-lined the last minute, channels are locked\n"));
  else if (!strcmp(cfgent->name, STR("fight-threshold")))
    dprintf(idx, STR("When more than fight-threshold ops/deops/kicks/bans/unbans altogether have happened on a channel in one minute, the channel is locked\n"));
  else {
    dprintf(idx, STR("No description for %s ???\n"), cfgent->name);
    putlog(LOG_ERRORS, "*", STR("getin_describe() called with unknown config entry %s"), cfgent->name);
  }
}

void getin_changed(struct cfg_entry *cfgent, char *oldval, int *valid)
{
  int i;

  if (!cfgent->gdata)
    return;
  *valid = 0;
  if (!strcmp(cfgent->name, STR("op-requests"))) {
    int L,
      R;
    char *value = cfgent->gdata;

    L = atoi(value);
    value = strchr(value, ':');
    if (!value)
      return;
    value++;
    R = atoi(value);
    if ((R >= 60) || (R <= 3) || (L < 1) || (L > R))
      return;
    *valid = 1;
    return;
  }
  if (!strcmp(cfgent->name, STR("lock-threshold"))) {
    int L,
      R;
    char *value = cfgent->gdata;

    L = atoi(value);
    value = strchr(value, ':');
    if (!value)
      return;
    value++;
    R = atoi(value);
    if ((R >= 1000) || (R < 0) || (L < 0) || (L > 100))
      return;
    *valid = 1;
    return;
  }
  i = atoi(cfgent->gdata);
  if (!strcmp(cfgent->name, STR("op-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("invite-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("key-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("limit-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("unban-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("lag-threshold"))) {
    if ((i < 3) || (i > 60))
      return;
  } else if (!strcmp(cfgent->name, STR("fight-threshold"))) {
    if (i && ((i < 50) || (i > 1000)))
      return;
  } else if (!strcmp(cfgent->name, STR("kill-threshold"))) {
    if ((i < 0) || (i >= 200))
      return;
  } else if (!strcmp(cfgent->name, STR("op-time-slack"))) {
    if ((i < 30) || (i > 1200))
      return;
  }
  *valid = 1;
  return;
}

struct cfg_entry CFG_OPBOTS = {
  "op-bots", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

#ifdef S_AUTOLOCK
struct cfg_entry CFG_FIGHTTHRESHOLD = {
  "fight-threshold", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};
#endif /* S_AUTOLOCK */

struct cfg_entry CFG_INBOTS = {
  "in-bots", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_LAGTHRESHOLD = {
  "lag-threshold", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_OPREQUESTS = {
  "op-requests", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_OPTIMESLACK = {
  "op-time-slack", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};
#endif /* HUB */


void add_cfg(struct cfg_entry *entry)
{
  cfg = (void *) nrealloc(cfg, sizeof(void *) * (cfg_count + 1));
  cfg[cfg_count] = entry;
  cfg_count++;
  entry->ldata = NULL;
  entry->gdata = NULL;
}

struct cfg_entry *check_can_set_cfg(char *target, char *entryname)
{
  int i;
  struct userrec *u;
  struct cfg_entry *entry = NULL;

  for (i = 0; i < cfg_count; i++)
    if (!strcmp(cfg[i]->name, entryname)) {
      entry = cfg[i];
      break;
    }
  if (!entry)
    return 0;
  if (target) {
    if (!(entry->flags & CFGF_LOCAL))
      return 0;
    if (!(u = get_user_by_handle(userlist, target)))
      return 0;
    if (!(u->flags & USER_BOT))
      return 0;
  } else {
    if (!(entry->flags & CFGF_GLOBAL))
      return 0;
  }
  return entry;
}

void set_cfg_str(char *target, char *entryname, char *data)
{
  struct cfg_entry *entry;

  if (!(entry = check_can_set_cfg(target, entryname)))
    return;
  if (data && !strcmp(data, "-"))
    data = NULL;
  if (data && (strlen(data) >= 1024))
    data[1023] = 0;
  if (target) {
    struct userrec *u = get_user_by_handle(userlist, target);
    struct xtra_key *xk;
    char *olddata = entry->ldata;

    if (u && !strcmp(botnetnick, u->handle)) {
      if (data) {
        entry->ldata = nmalloc(strlen(data) + 1);
        strcpy(entry->ldata, data);
      } else
        entry->ldata = NULL;
      if (entry->localchanged) {
        int valid = 1;

        entry->localchanged(entry, olddata, &valid);
        if (!valid) {
          if (entry->ldata)
            nfree(entry->ldata);
          entry->ldata = olddata;
          data = olddata;
          olddata = NULL;
        }
      }
    }
    xk = user_malloc(sizeof(struct xtra_key));
    egg_bzero(xk, sizeof(struct xtra_key));
    xk->key = user_malloc(strlen(entry->name) + 1);
    strcpy(xk->key, entry->name);
    if (data) {
      xk->data = user_malloc(strlen(data) + 1);
      strcpy(xk->data, data);
    }
    set_user(&USERENTRY_CONFIG, u, xk);
    if (olddata)
      nfree(olddata);
  } else {
    char *olddata = entry->gdata;

    if (data) {
      entry->gdata = nmalloc(strlen(data) + 1);
      strcpy(entry->gdata, data);
    } else
      entry->gdata = NULL;
    if (entry->globalchanged) {
      int valid = 1;

      entry->globalchanged(entry, olddata, &valid);
      if (!valid) {
        if (entry->gdata)
          nfree(entry->gdata);
        entry->gdata = olddata;
        olddata = NULL;
      }
    }
    if (!cfg_noshare)
      botnet_send_cfg_broad(-1, entry);
    if (olddata)
      nfree(olddata);
  }
}

/* Expected memory usage
 */
int expmem_config()
{
  int tot = 0, i;
#ifdef S_DCCPASS
{
  struct cmd_pass *cp = NULL;
  for (cp = cmdpass; cp; cp = cp->next) {
    tot += sizeof(struct cmd_pass) + strlen(cp->name) + 1;
  }
}
#endif /* S_DCCPASS */
  for (i = 0; i < cfg_count; i++) {
    tot += sizeof(void *);
    if (cfg[i]->gdata)
      tot += strlen(cfg[i]->gdata) + 1;
    if (cfg[i]->ldata)
      tot += strlen(cfg[i]->ldata) + 1;
  }
  return tot;
}

void init_config()
{
#ifdef S_AUTH
  add_cfg(&CFG_AUTHKEY);
#endif /* S_AUTH */
  add_cfg(&CFG_MOTD);
  add_cfg(&CFG_FORKINTERVAL);
#ifdef S_LASTCHECK
  add_cfg(&CFG_LOGIN);
#endif /* S_LASTCHECK */
#ifdef S_HIJACKCHECK
  add_cfg(&CFG_HIJACK);
#endif /* S_HIJACKCHECK */
#ifdef S_ANTITRACE
  add_cfg(&CFG_TRACE);
#endif /* S_ANTITRACE */
#ifdef S_PROMISC
  add_cfg(&CFG_PROMISC);
#endif /* S_PROMISC */
#ifdef S_PROCESSCHECK
  add_cfg(&CFG_BADPROCESS);
  add_cfg(&CFG_PROCESSLIST);
#endif /* S_PROCESSCHECK */
  add_cfg(&CFG_BADCOOKIE);
  add_cfg(&CFG_MANUALOP);
#ifdef G_MEAN
  add_cfg(&CFG_MEANDEOP);
  add_cfg(&CFG_MEANKICK);
  add_cfg(&CFG_MEANBAN);
#endif /* G_MEAN */
  add_cfg(&CFG_MDOP);
  add_cfg(&CFG_MOP);
#ifdef S_MSGCMDS
  add_cfg(&CFG_MSGOP);
  add_cfg(&CFG_MSGPASS);
  add_cfg(&CFG_MSGINVITE);
#endif /* S_MSGCMDS */
#ifdef HUB
  add_cfg(&CFG_NICK);
  add_cfg(&CFG_SERVERS);
  add_cfg(&CFG_SERVERS6);
  add_cfg(&CFG_REALNAME);
  set_cfg_str(NULL, STR("realname"), "A deranged product of evil coders");
  add_cfg(&CFG_OPBOTS);
  add_cfg(&CFG_INBOTS);
  add_cfg(&CFG_LAGTHRESHOLD);
  add_cfg(&CFG_OPREQUESTS);
  add_cfg(&CFG_OPTIMESLACK);
#ifdef S_AUTOLOCK
  add_cfg(&CFG_FIGHTTHRESHOLD);
#endif /* S_AUTOLOCK */
#endif /* HUB */
}

#ifdef S_DCCPASS
int check_cmd_pass(char *cmd, char *pass)
{
  struct cmd_pass *cp;

  for (cp = cmdpass; cp; cp = cp->next)
    if (!egg_strcasecmp(cmd, cp->name)) {
      char tmp[32];

      encrypt_pass(pass, tmp);
      if (!strcmp(tmp, cp->pass))
        return 1;
      return 0;
    }
  return 0;
}

int has_cmd_pass(char *cmd) 
{
  struct cmd_pass *cp;

  for (cp = cmdpass; cp; cp = cp->next)
    if (!egg_strcasecmp(cmd, cp->name))
      return 1;
  return 0;
}

void set_cmd_pass(char *ln, int shareit) 
{
  struct cmd_pass *cp;
  char *cmd;

  cmd = newsplit(&ln);
  for (cp = cmdpass; cp; cp = cp->next)
    if (!strcmp(cmd, cp->name))
      break;
  if (cp)
    if (ln[0]) {
      /* change */
      strcpy(cp->pass, ln);
      if (shareit)
        botnet_send_cmdpass(-1, cp->name, cp->pass);
    } else {
      if (cp == cmdpass)
        cmdpass = cp->next;
      else {
        struct cmd_pass *cp2;

        cp2 = cmdpass;
        while (cp2->next != cp)
          cp2 = cp2->next;
        cp2->next = cp->next;
      }
      if (shareit)
        botnet_send_cmdpass(-1, cp->name, "");
      nfree(cp->name);
      nfree(cp);
  } else if (ln[0]) {
    /* create */
    cp = nmalloc(sizeof(struct cmd_pass));
    cp->next = cmdpass;
    cmdpass = cp;
    cp->name = nmalloc(strlen(cmd) + 1);
    strcpy(cp->name, cmd);
    strcpy(cp->pass, ln);
    if (shareit)
      botnet_send_cmdpass(-1, cp->name, cp->pass);
  }
}
#endif /* S_DCCPAS */

void userfile_cfg_line(char *ln)
{
  char *name;
  int i;
  struct cfg_entry *cfgent = NULL;

  name = newsplit(&ln);
  for (i = 0; !cfgent && (i < cfg_count); i++)
    if (!strcmp(cfg[i]->name, name))
      cfgent = cfg[i];
  if (cfgent) {
    set_cfg_str(NULL, cfgent->name, ln[0] ? ln : NULL);
  } else
    putlog(LOG_ERRORS, "*", STR("Unrecognized config entry %s in userfile"), name);

}

void got_config_share(int idx, char *ln)
{
  char *name;
  int i;
  struct cfg_entry *cfgent = NULL;

  cfg_noshare = 1;
  name = newsplit(&ln);
  for (i = 0; !cfgent && (i < cfg_count); i++)
    if (!strcmp(cfg[i]->name, name))
      cfgent = cfg[i];
  if (cfgent) {
    set_cfg_str(NULL, cfgent->name, ln[0] ? ln : NULL);
    botnet_send_cfg_broad(idx, cfgent);
  } else
    putlog(LOG_ERRORS, "*", STR("Unrecognized config entry %s in userfile"), name);
  cfg_noshare = 0;
}

void trigger_cfg_changed()
{
  int i;
  struct userrec *u;
  struct xtra_key *xk;

  u = get_user_by_handle(userlist, botnetnick);

  for (i = 0; i < cfg_count; i++) {
    if (cfg[i]->flags & CFGF_LOCAL) {
      xk = get_user(&USERENTRY_CONFIG, u);
      while (xk && strcmp(xk->key, cfg[i]->name))
	xk = xk->next;
      if (xk) {
	putlog(LOG_DEBUG, "*", STR("trigger_cfg_changed for %s"), cfg[i]->name ? cfg[i]->name : "(null)");
	if (!strcmp(cfg[i]->name, xk->key ? xk->key : "")) {
	  set_cfg_str(botnetnick, cfg[i]->name, xk->data);
	}
      }
    }
  }
}
