/*
 * config.c -- handles:
 *   botnet config
 *   cmdpass functions
 */

#include "common.h"
#include "cfg.h"
#include "cmds.h"
#include "userrec.h"
#include "auth.h"
#include "misc.h"
#include "users.h"
#include "dccutil.h"
#include "botmsg.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include "chan.h"
#include "tandem.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/ctcp.mod/ctcp.h"
#ifdef LEAF
#include "src/chanprog.h"
#include "src/mod/server.mod/server.h"
#endif /* LEAF */
#include "botnet.h"
#include <net/if.h>

#include "stat.h"

int 				cfg_count = 0, cfg_noshare = 0;
struct cfg_entry 		**cfg = NULL;
char 				cmdprefix = '+';	/* This is the prefix for msg/channel cmds */
struct cmd_pass 		*cmdpass = NULL;

#ifdef HUB
static void chanset_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("chanset is a list of default options for when a channel is added. Same format as .chanset.\n"));
}
#endif /* HUB */

#ifdef LEAF
static void chanset_changed(struct cfg_entry *entry, int *valid) {
  if (entry->ldata)
    strncpyz(cfg_glob_chanset, (char *) entry->ldata, 512);
  else if (entry->gdata)
    strncpyz(cfg_glob_chanset, (char *) entry->gdata, 512);
}
#endif /* LEAF */

struct cfg_entry CFG_CHANSET = {
	"chanset", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL, 
#ifdef LEAF
	chanset_changed, chanset_changed
#else
	NULL, NULL, chanset_describe
#endif /* LEAF */
};

#ifdef HUB
static void servport_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("servport is the default port to use for server connections.\n"));
}
#endif /* HUB */

#ifdef LEAF
static void servport_changed(struct cfg_entry *entry, int *valid) {
  if (entry->ldata)
    default_port = atoi(entry->ldata);
  else if (entry->gdata)
    default_port = atoi(entry->gdata);
}
#endif /* LEAF */

struct cfg_entry CFG_SERVPORT = {
	"servport", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL, 
#ifdef LEAF
	servport_changed, servport_changed
#else
	NULL, NULL, servport_describe
#endif /* LEAF */
};

#ifdef HUB
static void authkey_describe(struct cfg_entry *entry, int idx) {
  dprintf(idx, 
  STR("authkey is used for authing, give to your users if they are to use DCC chat or IRC cmds. (can be bot specific)\n"));
}
#endif /* HUB */

struct cfg_entry CFG_AUTHKEY = {
	"authkey", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
#ifdef LEAF
	NULL, NULL
#else
	NULL, NULL, authkey_describe
#endif /* LEAF */
};

#ifdef HUB
static void msgcmds_describe(struct cfg_entry *entry, int idx) {
  if (entry == &CFG_MSGOP)
    dprintf(idx, STR("msgop defines the cmd for opping via msging the bot (leave blank to disable)\n"));
  else if (entry == &CFG_MSGPASS)
    dprintf(idx, STR("msgpass defines the cmd for setting a pass via msging the bot (leave blank to disable)\n"));
  else if (entry == &CFG_MSGINVITE)
    dprintf(idx, STR("msginvite defines the cmd for requesting invite via msging the bot (leave blank to disable)\n"));
  else if (entry == &CFG_MSGIDENT)
    dprintf(idx, STR("msgident defines the cmd for identing via msging the bot (leave blank to disable)\n"));
}
#endif /* HUB */

struct cfg_entry CFG_MSGOP = {
	"msgop", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
	NULL, NULL
#ifdef HUB
	, msgcmds_describe
#endif /* HUB */

};

struct cfg_entry CFG_MSGPASS = {
	"msgpass", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
	NULL, NULL
#ifdef HUB
	, msgcmds_describe
#endif /* HUB */
};

struct cfg_entry CFG_MSGINVITE = {
	"msginvite", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
	NULL, NULL
#ifdef HUB
	, msgcmds_describe
#endif /* HUB */
};

struct cfg_entry CFG_MSGIDENT = {
	"msgident", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
	NULL, NULL
#ifdef HUB
	, msgcmds_describe
#endif /* HUB */
};

#ifdef HUB
static void cmdprefix_describe(struct cfg_entry *entry, int idx) {
  dprintf(idx, STR("cmdprefix is the prefix character used for msg cmds, ie: !op or .op\n"));
}
#endif /* HUB */

#ifdef LEAF
static void cmdprefix_changed(struct cfg_entry *entry, int *valid) {
  if (entry->ldata)
    cmdprefix = entry->ldata[0];
  else if (entry->gdata)
    cmdprefix = entry->gdata[0];
}
#endif /* LEAF */

struct cfg_entry CFG_CMDPREFIX = {
	"cmdprefix", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
#ifdef LEAF
	cmdprefix_changed, cmdprefix_changed
#else
	NULL, NULL, cmdprefix_describe
#endif /* LEAF */
};

void deflag_user(struct userrec *u, int why, char *msg, struct chanset_t *chan)
{
  char tmp[256] = "", tmp2[1024] = "";
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  int which = 0;

  if (!u)
    return;

  switch (why) {
  case DEFLAG_BADCOOKIE:
    strcpy(tmp, "Bad op cookie");
    which = chan->bad_cookie;
    break;
  case DEFLAG_MANUALOP:
    strcpy(tmp, STR("Manual op in -manop channel"));
    which = chan->manop;
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
    strcpy(tmp, "Mass deop");
    which = chan->mdop;
    break;
  case DEFLAG_MOP:
    strcpy(tmp, "Mass op");
    which = chan->mop;
    break;
  default:
    sprintf(tmp, "Reason #%i", why);
  }
  if (which == P_DEOP) {
    putlog(LOG_WARN, "*",  "Setting %s +d (%s): %s", u->handle, tmp, msg);
    sprintf(tmp2, "+d: %s (%s)", tmp, msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
    get_user_flagrec(u, &fr, chan->dname);
    fr.global = USER_DEOP;
    fr.chan = USER_DEOP;
    set_user_flagrec(u, &fr, chan->dname);
  } else if (which == P_KICK) {
    putlog(LOG_WARN, "*",  "Setting %s +dk (%s): %s", u->handle, tmp, msg);
    sprintf(tmp2, "+dk: %s (%s)", tmp, msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
    get_user_flagrec(u, &fr, chan->dname);
    fr.global = USER_DEOP | USER_KICK;
    fr.chan = USER_DEOP | USER_KICK;
    set_user_flagrec(u, &fr, chan->dname);
  } else if (which == P_DELETE) {
    putlog(LOG_WARN, "*",  "Deleting %s (%s): %s", u->handle, tmp, msg);
    deluser(u->handle);
  } else {
    putlog(LOG_WARN, "*",  "No user flag effects for %s (%s): %s", u->handle, tmp, msg);
    sprintf(tmp2, "Warning: %s (%s)", tmp, msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
  }
}

#ifdef HUB
static void misc_describe(struct cfg_entry *cfgent, int idx)
{
  int i = 0;

  if (!strcmp(cfgent->name, "fork-interval")) {
    dprintf(idx, STR("fork-interval is number of seconds in between each fork() call made by the bot, to change process ID and reset cpu usage counters.\n"));
    i = 1;
  } else if (!strcmp(cfgent->name, "login")) {
    dprintf(idx, STR("login sets how to handle someone logging in to the shell\n"));
  } else if (!strcmp(cfgent->name, "trace")) {
    dprintf(idx, STR("trace sets how to handle someone tracing/debugging the bot\n"));
  } else if (!strcmp(cfgent->name, "promisc")) {
    dprintf(idx, STR("promisc sets how to handle when a interface is set to promiscuous mode\n"));
  } else if (!strcmp(cfgent->name, "bad-process")) {
    dprintf(idx, STR("bad-process sets how to handle when a running process not listed in process-list is detected\n"));
  } else if (!strcmp(cfgent->name, "process-list")) {
    dprintf(idx, STR("process-list is a comma-separated list of \"expected processes\" running on the bots uid\n"));
    i = 1;
  } else if (!strcmp(cfgent->name, "hijack")) {
    dprintf(idx, STR("hijack sets how to handle when a commonly used hijack method attempt is detected. (recommended: die)\n"));
  }
  if (!i)
    dprintf(idx, "Valid settings are: ignore, warn, die, reject, suicide\n");
}
#endif /* HUB */

static void fork_lchanged(struct cfg_entry * cfgent, int * valid) {
  if (!cfgent->ldata)
    return;
  if (atoi(cfgent->ldata) <= 0)
    *valid = 0;
}

static void fork_gchanged(struct cfg_entry * cfgent, int * valid) {
  if (!cfgent->gdata)
    return;
  if (atoi(cfgent->gdata) <= 0)
    *valid = 0;
}

#ifdef HUB
static void fork_describe(struct cfg_entry * cfgent, int idx) {
  dprintf(idx, STR("fork-interval is number of seconds in between each fork() call made by the bot, to change process ID and reset cpu usage counters.\n"));
}
#endif /* HUB */

struct cfg_entry CFG_FORKINTERVAL = {
	"fork-interval", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
	fork_gchanged, fork_lchanged
#ifdef HUB
	, fork_describe
#endif /* HUB */
};

static void detect_lchanged(struct cfg_entry * cfgent, int * valid) {
  char *p = NULL;

  if (!(p = (char *) cfgent->ldata))
    *valid = 1;
  else if (strcmp(p, "ignore") && strcmp(p, "die") && strcmp(p, "reject") && strcmp(p, "suicide") && strcmp(p, "warn"))
    *valid = 0;
}

static void detect_gchanged(struct cfg_entry * cfgent, int * valid) {
  char *p = (char *) cfgent->ldata;
  if (!p)
    *valid=1;
  else if (strcmp(p, "ignore") && strcmp(p, "die") && strcmp(p, "reject") && strcmp(p, "suicide") && strcmp(p, "warn"))
    *valid=0;
}

struct cfg_entry CFG_LOGIN = {
	"login", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
	detect_gchanged, detect_lchanged
#ifdef HUB
	, misc_describe
#endif /* HUB */
};
struct cfg_entry CFG_HIJACK = {
	"hijack", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
	detect_gchanged, detect_lchanged
#ifdef HUB
	, misc_describe
#endif /* HUB */
};
struct cfg_entry CFG_TRACE = {
	"trace", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
	detect_gchanged, detect_lchanged
#ifdef HUB
	, misc_describe
#endif /* HUB */
};
struct cfg_entry CFG_PROMISC = {
	"promisc", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
	detect_gchanged, detect_lchanged
#ifdef HUB
	, misc_describe
#endif /* HUB */
};
struct cfg_entry CFG_BADPROCESS = {
	"bad-process", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
	detect_gchanged, detect_lchanged
#ifdef HUB
	, misc_describe
#endif /* HUB */
};

struct cfg_entry CFG_PROCESSLIST = {
	"process-list", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
	NULL, NULL
#ifdef HUB
	, misc_describe
#endif /* HUB */
};

#ifdef LEAF
static void servers_changed(struct cfg_entry * entry, int * valid) {
  char *slist = NULL, *p = NULL;

  if (!strcmp(entry->name, "servers")) {
    if (conf.bot->host6 || conf.bot->ip6) /* we want to use the servers6 entry. */
      return;
  } else if (!strcmp(entry->name, "servers6")) {
    if (!conf.bot->host6 && !conf.bot->ip6) /* we probably want to use the normal server list.. */
      return;
  }
  slist = (char *) (entry->ldata ? entry->ldata : (entry->gdata ? entry->gdata : ""));
  if (serverlist) {
    clearq(serverlist);
    serverlist = NULL;
  }
  shuffle(slist, ",");
  p = strdup(slist);
  add_server(p);
  free(p);
}
#endif /* LEAF */

#ifdef HUB
static void servers_describe(struct cfg_entry * entry, int idx) {
  if (!strcmp(entry->name, "servers")) 
    dprintf(idx, STR("servers is a comma-separated list of servers the bot will use\n"));
  else
    dprintf(idx, STR("servers6 is a comma-separated list of servers the bot will use (FOR IPv6)\n"));
}
#endif /* HUB */

struct cfg_entry CFG_SERVERS = {
	"servers", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
#ifdef LEAF
	servers_changed, servers_changed
#else
	NULL, NULL, servers_describe
#endif /* LEAF */
};

struct cfg_entry CFG_SERVERS6 = {
	"servers6", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
#ifdef LEAF
	servers_changed, servers_changed
#else
	NULL, NULL, servers_describe
#endif /* LEAF */
};

#ifdef LEAF
static void nick_changed(struct cfg_entry * entry, int * valid) {
  char *p = NULL;

  if (entry->ldata)
    p = entry->ldata;
  else if (entry->gdata)
    p = entry->gdata;
  else
    p = NULL;
  if (p && p[0])
    strncpyz(origbotname, p, NICKLEN + 1);
  else
    strncpyz(origbotname, conf.bot->nick, NICKLEN + 1);
  if (server_online)
    dprintf(DP_SERVER, "NICK %s\n", origbotname);
}
#endif /* LEAF */

#ifdef HUB
static void nick_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("nick is the bots preferred nick when connecting/using .resetnick\n"));
}
#endif /* HUB */

struct cfg_entry CFG_NICK = {
	"nick", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
#ifdef LEAF
	nick_changed, nick_changed
#else
	NULL, NULL, nick_describe
#endif /* LEAF */
};

#ifdef HUB
static void realname_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("realname is the bots \"real name\" when connecting\n"));
}
#endif /* HUB */

#ifdef LEAF
static void realname_changed(struct cfg_entry * entry, int * valid) {
  if (entry->ldata)
    strncpyz(botrealname, (char *) entry->ldata, 121);
  else if (entry->gdata)
    strncpyz(botrealname, (char *) entry->gdata, 121);
}
#endif /* LEAF */

struct cfg_entry CFG_REALNAME = {
	"realname", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
#ifdef LEAF
	realname_changed, realname_changed
#else
	NULL, NULL, realname_describe
#endif /* LEAF */
};

#ifdef HUB
static void dccauth_describe(struct cfg_entry *cfgent, int idx)
{
  dprintf(idx, STR("dccauth is boolean (0 or 1). Set to use auth checking on dcc/telnet login.\n"));
}
#endif /* HUB */

struct cfg_entry CFG_DCCAUTH = {
	"dccauth", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
	NULL, NULL
#ifdef HUB
	, dccauth_describe
#endif /* HUB */
};

#ifdef HUB
static void getin_describe(struct cfg_entry *cfgent, int idx)
{
  if (!strcmp(cfgent->name, "op-bots"))
    dprintf(idx, STR("op-bots is number of bots to ask every time a oprequest is to be made\n"));
  else if (!strcmp(cfgent->name, "in-bots"))
    dprintf(idx, STR("in-bots is number of bots to ask every time a inrequest is to be made\n"));
  else if (!strcmp(cfgent->name, "op-requests"))
    dprintf(idx, STR("op-requests (requests:seconds) limits how often the bot will ask for ops\n"));
  else if (!strcmp(cfgent->name, "lag-threshold"))
    dprintf(idx, STR("lag-threshold is maximum acceptable server lag for the bot to send/honor requests\n"));
  else if (!strcmp(cfgent->name, "op-time-slack"))
    dprintf(idx, STR("op-time-slack is number of seconds a opcookies encoded time value can be off from the bots current time\n"));
  else if (!strcmp(cfgent->name, "close-threshold"))
    dprintf(idx, STR("Format H:L. When at least H hubs but L or less leafs are linked, close all channels\n"));
  else if (!strcmp(cfgent->name, "kill-threshold"))
    dprintf(idx, STR("When more than kill-threshold bots have been killed/k-lined the last minute, channels are locked\n"));
  else if (!strcmp(cfgent->name, "fight-threshold"))
    dprintf(idx, STR("When more than fight-threshold ops/deops/kicks/bans/unbans altogether have happened on a channel in one minute, the channel is locked\n"));
  else {
    dprintf(idx, STR("No description for %s ???\n"), cfgent->name);
    putlog(LOG_ERRORS, "*", STR("getin_describe() called with unknown config entry %s"), cfgent->name);
  }
}
#endif /* HUB */

static void getin_changed(struct cfg_entry *cfgent, int *valid)
{
  int i;

  if (!cfgent->gdata)
    return;
  *valid = 0;
  if (!strcmp(cfgent->name, "op-requests")) {
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
  if (!strcmp(cfgent->name, "close-threshold")) {
    int L, R;
    char *value = NULL;

    value = cfgent->gdata;

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
  if (!strcmp(cfgent->name, "op-bots")) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, "invite-bots")) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, "key-bots")) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, "limit-bots")) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, "unban-bots")) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, "lag-threshold")) {
    if ((i < 3) || (i > 60))
      return;
  } else if (!strcmp(cfgent->name, "fight-threshold")) {
    if (i && ((i < 50) || (i > 1000)))
      return;
  } else if (!strcmp(cfgent->name, "kill-threshold")) {
    if ((i < 0) || (i >= 200))
      return;
  } else if (!strcmp(cfgent->name, "op-time-slack")) {
    if ((i < 30) || (i > 1200))
      return;
  }
  *valid = 1;
  return;
}

struct cfg_entry CFG_OPBOTS = {
	"op-bots", CFGF_GLOBAL, NULL, NULL,
	getin_changed, NULL
#ifdef HUB
	, getin_describe
#endif /* HUB */
};

struct cfg_entry CFG_FIGHTTHRESHOLD = {
	"fight-threshold", CFGF_GLOBAL, NULL, NULL,
	getin_changed, NULL
#ifdef HUB
	, getin_describe
#endif /* HUB */
};

struct cfg_entry CFG_CLOSETHRESHOLD = {
	"close-threshold", CFGF_GLOBAL, NULL, NULL,
	getin_changed, NULL
#ifdef HUB
	, getin_describe
#endif /* HUB */
};

struct cfg_entry CFG_KILLTHRESHOLD = {
	"kill-threshold", CFGF_GLOBAL, NULL, NULL,
	getin_changed, NULL
#ifdef HUB
	, getin_describe
#endif /* HUB */
};

struct cfg_entry CFG_INBOTS = {
	"in-bots", CFGF_GLOBAL, NULL, NULL,
	getin_changed, NULL
#ifdef HUB
	, getin_describe
#endif /* HUB */
};

struct cfg_entry CFG_LAGTHRESHOLD = {
	"lag-threshold", CFGF_GLOBAL, NULL, NULL,
	getin_changed, NULL
#ifdef HUB
	, getin_describe
#endif /* HUB */
};

struct cfg_entry CFG_OPREQUESTS = {
	"op-requests", CFGF_GLOBAL, NULL, NULL,
	getin_changed, NULL
#ifdef HUB
	, getin_describe
#endif /* HUB */
};

void add_cfg(struct cfg_entry *entry)
{
  cfg = (struct cfg_entry **) realloc(cfg, sizeof(void *) * (cfg_count + 1));
  cfg[cfg_count] = entry;
  cfg_count++;
  entry->ldata = NULL;
  entry->gdata = NULL;
}

static struct cfg_entry *check_can_set_cfg(char *target, char *entryname)
{
  int i;
  struct userrec *u = NULL;
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
    if (!u->bot)
      return 0;
  } else {
    if (!(entry->flags & CFGF_GLOBAL))
      return 0;
  }
  return entry;
}

void set_cfg_str(char *target, char *entryname, char *data)
{
  struct cfg_entry *entry = NULL;

  if (!(entry = check_can_set_cfg(target, entryname)))
    return;
  if (data && !strcmp(data, "-"))
    data = NULL;
  if (data && (strlen(data) >= 1024))
    data[1023] = 0;
  if (target) {
    struct userrec *u = get_user_by_handle(userlist, target);
    struct xtra_key *xk = NULL;
    char *olddata = entry->ldata;

    if (u && !strcmp(conf.bot->nick, u->handle)) {
      if (data) {
        entry->ldata = strdup(data);
      } else
        entry->ldata = NULL;
      if (entry->localchanged) {
        int valid = 1;

        /* entry->localchanged(entry, olddata, &valid); */
        entry->localchanged(entry, &valid);
        if (!valid) {
          if (entry->ldata)
            free(entry->ldata);
          entry->ldata = olddata;
          data = olddata;
          olddata = NULL;
        }
      }
    }
    xk = (struct xtra_key *) calloc(1, sizeof(struct xtra_key));
    xk->key = strdup(entry->name);
    if (data) {
      xk->data = strdup(data);
    }
    set_user(&USERENTRY_CONFIG, u, xk);
    if (olddata)
      free(olddata);
  } else {
    char *olddata = entry->gdata;

    if (data) {
      entry->gdata = strdup(data);
    } else
      entry->gdata = NULL;
    if (entry->globalchanged) {
      int valid = 1;

      /* entry->globalchanged(entry, olddata, &valid); */
      entry->globalchanged(entry, &valid);
      if (!valid) {
        if (entry->gdata)
          free(entry->gdata);
        entry->gdata = olddata;
        olddata = NULL;
      }
    }
    if (!cfg_noshare)
      botnet_send_cfg_broad(-1, entry);
    if (olddata)
      free(olddata);
  }
}

struct cfg_entry CFG_MOTD = { 
	"motd", CFGF_GLOBAL, NULL, NULL, 
	NULL, NULL
#ifdef HUB
	, NULL
#endif /* HUB */
};

void init_config()
{
  add_cfg(&CFG_AUTHKEY);
  add_cfg(&CFG_BADPROCESS);
  add_cfg(&CFG_DCCAUTH);
  add_cfg(&CFG_CHANSET);
  add_cfg(&CFG_CLOAK_SCRIPT);
  add_cfg(&CFG_CLOSETHRESHOLD);
  add_cfg(&CFG_CMDPREFIX);
  add_cfg(&CFG_FIGHTTHRESHOLD);
  add_cfg(&CFG_FORKINTERVAL);
  add_cfg(&CFG_HIJACK);
  add_cfg(&CFG_INBOTS);
  add_cfg(&CFG_KILLTHRESHOLD);
  add_cfg(&CFG_LAGTHRESHOLD);
  add_cfg(&CFG_LOGIN);
  add_cfg(&CFG_MOTD);
#ifdef G_MEAN
  add_cfg(&CFG_MEANBAN);
  add_cfg(&CFG_MEANDEOP);
  add_cfg(&CFG_MEANKICK);
#endif /* G_MEAN */
  add_cfg(&CFG_MSGIDENT);
  add_cfg(&CFG_MSGINVITE);
  add_cfg(&CFG_MSGOP);
  add_cfg(&CFG_MSGPASS);
  add_cfg(&CFG_NICK);
  add_cfg(&CFG_OPBOTS);
  add_cfg(&CFG_OPREQUESTS);
  add_cfg(&CFG_PROCESSLIST);
  add_cfg(&CFG_PROMISC);
  add_cfg(&CFG_REALNAME);
  add_cfg(&CFG_SERVERS);
  add_cfg(&CFG_SERVERS6);
  add_cfg(&CFG_SERVPORT);
  add_cfg(&CFG_TRACE);
}

int check_cmd_pass(const char *cmd, char *pass)
{
  struct cmd_pass *cp = NULL;

  for (cp = cmdpass; cp; cp = cp->next)
    if (!egg_strcasecmp(cmd, cp->name)) {
      char tmp[32] = "";

      encrypt_pass(pass, tmp);
      if (!strcmp(tmp, cp->pass))
        return 1;
      return 0;
    }
  return 0;
}

int has_cmd_pass(const char *cmd) 
{
  struct cmd_pass *cp = NULL;

  for (cp = cmdpass; cp; cp = cp->next)
    if (!egg_strcasecmp(cmd, cp->name))
      return 1;
  return 0;
}

void set_cmd_pass(char *ln, int shareit) 
{
  struct cmd_pass *cp = NULL;
  char *cmd = NULL;

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
      free(cp->name);
      free(cp);
  } else if (ln[0]) {
    /* create */
    cp = (struct cmd_pass *) calloc(1, sizeof(struct cmd_pass));
    cp->next = cmdpass;
    cmdpass = cp;
    cp->name = strdup(cmd);
    strcpy(cp->pass, ln);
    if (shareit)
      botnet_send_cmdpass(-1, cp->name, cp->pass);
  }
}

void userfile_cfg_line(char *ln)
{
  char *name = NULL;
  int i;
  struct cfg_entry *cfgent = NULL;

  name = newsplit(&ln);
  for (i = 0; !cfgent && (i < cfg_count); i++)
    if (!strcmp(cfg[i]->name, name))
      cfgent = cfg[i];
  if (cfgent) {
    /* if we are a leaf, dont share the cfg line. */
    if (bot_hublevel(conf.bot->u) == 999)
      cfg_noshare = 1;
    set_cfg_str(NULL, cfgent->name, (ln && ln[0]) ? ln : NULL);
    /* regardless of leaf/hub it is safe to set this to 0 */
    cfg_noshare = 0;
  } else
    putlog(LOG_ERRORS, "*", "Unrecognized config entry %s in userfile", name);

}

void got_config_share(int idx, char *ln, int broad)
{
  char *name = NULL;
  int i;
  struct cfg_entry *cfgent = NULL;

  cfg_noshare = 1;
  name = newsplit(&ln);
  for (i = 0; !cfgent && (i < cfg_count); i++)
    if (!strcmp(cfg[i]->name, name))
      cfgent = cfg[i];
  if (cfgent) {
    set_cfg_str(NULL, cfgent->name, (ln && ln[0]) ? ln : NULL);
    if (broad)
      botnet_send_cfg_broad(idx, cfgent);
  } else
    putlog(LOG_ERRORS, "*", "Unrecognized config entry %s in userfile", name);
  cfg_noshare = 0;
}

void trigger_cfg_changed()
{
  int i;
  struct xtra_key *xk = NULL;

  for (i = 0; i < cfg_count; i++) {
    if (cfg[i]->flags & CFGF_LOCAL) {
      xk = get_user(&USERENTRY_CONFIG, conf.bot->u);
      while (xk && strcmp(xk->key, cfg[i]->name))
	xk = xk->next;
      if (xk) {
	putlog(LOG_DEBUG, "*", "trigger_cfg_changed for %s", cfg[i]->name ? cfg[i]->name : "(null)");
	if (!strcmp(cfg[i]->name, xk->key ? xk->key : "")) {
	  set_cfg_str(conf.bot->nick, cfg[i]->name, xk->data);
	}
      }
    }
  }
}
