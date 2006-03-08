/*
 * set.c
 *
 */


#include "common.h"
#include "set.h"
#include "shell.h"
#include "botmsg.h"
#include "chanprog.h"
#include "misc.h"
#include "src/mod/server.mod/server.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/ctcp.mod/ctcp.h"
#include "users.h"
#include "userrec.h"
#include "userent.h"
#include "rfc1459.h"

int set_noshare = 0;
int dcc_autoaway = 1800;
bool auth_obscure = 0;
bool auth_chan = 1;
char alias[1024] = "bc botcmd,bl botcmd ?,+list set +,-list set -,list set list";
char auth_key[51] = "";
char auth_prefix[2] = "";
int badprocess = DET_IGNORE;
bool dccauth = 0;
char *def_chanset = "+enforcebans +dynamicbans +userbans -bitch -protectops +cycle -inactive +userexempts -dynamicexempts +userinvites -dynamicinvites -nodesynch -closed -take -voice -private -fastop";
int cloak_script = 0;
rate_t close_threshold = { 0, 0 };
int fight_threshold;
int fork_interval;
int hijack = DET_DIE;
int in_bots = 2;
int ison_time = 10;
int kill_threshold;
int lag_threshold = 15;
int login = DET_WARN;
char motd[512] = "";
char msgident[21] = "";
char msginvite[21] = "";
char msgop[21] = "";
char msgpass[21] = "";
int op_bots = 1;
rate_t op_requests = { 2, 5 };
char process_list[1024] = "";
int promisc = DET_WARN;
int trace = DET_DIE;
bool offensive_bans = 1;
bool manop_warn = 1;
char homechan[51] = "";

static variable_t vars[] = {
 VAR("alias", 		alias,			sizeof(alias),			VAR_STRING|VAR_LIST|VAR_NOLOC|VAR_PERM),
 VAR("auth-chan",	&auth_chan,		0,				VAR_INT|VAR_BOOL|VAR_NOLHUB),
 VAR("auth-key",	auth_key,		sizeof(auth_key),		VAR_STRING|VAR_PERM),
 VAR("auth-prefix",	auth_prefix,		sizeof(auth_prefix),		VAR_STRING|VAR_NOLHUB|VAR_PERM),
 VAR("auth-obscure",	&auth_obscure,		0,				VAR_INT|VAR_BOOL),
 VAR("dcc-autoaway",	&dcc_autoaway,		0,				VAR_INT|VAR_NOLOC),	
 VAR("bad-process",	&badprocess,		0,				VAR_INT|VAR_DETECTED),
 VAR("dccauth",		&dccauth,		0,				VAR_INT|VAR_BOOL),
 VAR("chanset",		glob_chanset,		sizeof(glob_chanset),		VAR_STRING|VAR_CHANSET|VAR_NOLHUB),
 VAR("cloak-script",	&cloak_script,		0,				VAR_INT|VAR_CLOAK|VAR_NOLHUB),
 VAR("close-threshold",	&close_threshold,	0,				VAR_RATE|VAR_NOLOC),
 VAR("fight-threshold",	&fight_threshold,	0,				VAR_INT|VAR_NOLOC),
 VAR("flood-msg",	&flood_msg,		0,				VAR_RATE|VAR_NOLHUB),
 VAR("flood-ctcp",	&flood_ctcp,		0,				VAR_RATE|VAR_NOLHUB),
 VAR("flood-g",		&flood_g,		0,				VAR_RATE|VAR_NOLHUB),
 VAR("fork-interval",	&fork_interval,		0,				VAR_INT),
 VAR("hijack",		&hijack,		0,				VAR_INT|VAR_DETECTED|VAR_PERM),
 VAR("homechan",	homechan,		sizeof(homechan),		VAR_STRING|VAR_NOLOC|VAR_HIDE),
 VAR("in-bots",		&in_bots,		0,				VAR_INT|VAR_NOLOC),
 VAR("notify-time",	&ison_time,		0, 				VAR_INT|VAR_NOLHUB),
 VAR("kill-threshold",	&kill_threshold,	0,				VAR_INT|VAR_NOLOC),
 VAR("lag-threshold",	&lag_threshold,		0,				VAR_INT|VAR_NOLHUB),
 VAR("login",		&login,			0,				VAR_INT|VAR_DETECTED),
 VAR("manop-warn",	&manop_warn,		0,				VAR_INT|VAR_BOOL|VAR_NOLHUB),
 VAR("mean-kicks",	&offensive_bans,	0,				VAR_INT|VAR_BOOL|VAR_NOLHUB),
 VAR("motd",		motd,			sizeof(motd),			VAR_STRING|VAR_HIDE|VAR_NOLOC),
 VAR("msg-ident",	msgident,		sizeof(msgident),		VAR_STRING|VAR_NOLHUB),
 VAR("msg-invite",	msginvite,		sizeof(msginvite),		VAR_STRING|VAR_NOLHUB),
 VAR("msg-op",		msgop,			sizeof(msgop),			VAR_STRING|VAR_NOLHUB),
 VAR("msg-pass",	msgpass,		sizeof(msgpass),		VAR_STRING|VAR_NOLHUB),
 VAR("nick",		origbotname,		sizeof(origbotname),		VAR_STRING|VAR_NOLHUB|VAR_NICK|VAR_NODEF|VAR_NOGHUB),
 VAR("op-bots",		&op_bots,		0,				VAR_INT|VAR_NOLOC),
 VAR("op-requests",	&op_requests,		0,				VAR_RATE|VAR_NOLOC),
 VAR("process-list",	process_list,		sizeof(process_list),		VAR_STRING|VAR_LIST),
 VAR("promisc",		&promisc,		0,				VAR_INT|VAR_DETECTED),
 VAR("realname",	botrealname,		sizeof(botrealname),		VAR_STRING|VAR_NOLHUB),
 VAR("server-port",	&default_port,		0,				VAR_INT|VAR_NOLHUB),
 VAR("servers",		&serverlist,		0,				VAR_SERVERS|VAR_LIST|VAR_SHUFFLE|VAR_NOLHUB),
 VAR("servers6",	&serverlist,		0,				VAR_SERVERS|VAR_LIST|VAR_SHUFFLE|VAR_NOLHUB),
 VAR("trace",		&trace,			0,				VAR_INT|VAR_DETECTED),
 {NULL,			NULL,			0,				0, NULL, NULL, 0}
};


static bool use_server_type(const char *name)
{
  if (!conf.bot->hub) {
    if (!strcmp(name, "servers")) {
      if (conf.bot->net.host6 || conf.bot->net.ip6) /* we want to use the servers6 entry. */
        return 0;
    } else if (!strcmp(name, "servers6")) {
      if (!conf.bot->net.host6 && !conf.bot->net.ip6) /* we probably want to use the normal server list.. */
        return 0;
    }
  }
  return 1;
}

const char *var_find_by_mem(void *mem)
{
  int i = 0;

  for (i = 0; vars[i].name; i++) {
    if (vars[i].mem == mem)
      return vars[i].name;
  }
  return "";  
}


/* set the ptr to the new data */
static bool var_set_mem(variable_t *var, const char *datain)
{
  char *data = (datain && datain[0]) ? strdup(datain) : NULL, *datap = data;
sdprintf("var (mem): %s -> %s", var->name, datain);

  if (data && var->flags & VAR_SHUFFLE) {
//    char *datadup = strdup(data);

//    shuffle(datadup, ",");
//    data = datadup;
//    freedata++;
    shuffle(data, ",");
  }

  /* figure out it's type and set it's variable to the data */
  if (var->flags & VAR_INT) {
    bool isnumber = 0;
    int number = 0;

    isnumber = data ? str_isdigit(data) : 0;
    if (isnumber)
      number = atoi(data);
    else if (!isnumber && (var->flags & VAR_DETECTED)) {
      number = data ? det_translate(data) : DET_IGNORE;
#ifndef DEBUG
      if (number < 2 && !strcmp(var->name, "trace"))
        number = DET_DIE;
#endif
    }
    else if (!isnumber)
      number = 0;

    if (var->flags & VAR_CLOAK && !conf.bot->hub) {
      if (number == 0)
        number = randint(CLOAK_COUNT - 1) + 1;
    }

    *(int *) (var->mem) = number;

    if (var->flags & VAR_CLOAK && !conf.bot->hub)
      scriptchanged();
  } else if (var->flags & VAR_BOOL) {
    if (data && str_isdigit(data)) {
      int num = atoi(data);

      if (num == 0 || num == 1)
        *(bool *) (var->mem) = (bool) num;
    } else if (!data) {
      *(bool *) (var->mem) = 0;
    } else
      goto end;
  } else if (var->flags & VAR_STRING) {
    if (data)
      strlcpy((char *) var->mem, data, var->size);
    else
      ((char *) var->mem)[0] = 0;

    if (var->flags & VAR_NICK && !conf.bot->hub) {
       if (!data)
         strlcpy((char *) var->mem, conf.bot->nick, var->size);
       if (server_online && rfc_casecmp(botname, (char *) var->mem))
         dprintf(DP_SERVER, "NICK %s\n", (char *) var->mem);
    }
  } else if (var->flags & VAR_RATE) {
    char *p = NULL;
    
    if (data && (p = strchr(data, ':'))) {

      *p = 0;
      p++;
 
      (*(rate_t *) (var->mem)).count = atoi(data);
      (*(rate_t *) (var->mem)).time = atoi(p);
    } else if (!data) {
      (*(rate_t *) (var->mem)).count = 0;
      (*(rate_t *) (var->mem)).time = 0;
    } else
      goto end;
  } else if (var->flags & VAR_SERVERS) {
    if (!use_server_type(var->name))
      goto end;

    if (var->mem && *(struct server_list **)var->mem) {
      clearq(*(struct server_list **) var->mem);
      *(struct server_list **)var->mem = NULL;
    }

    if (data)
      add_server(data);
    
    if (server_online) {
      curserv = -1;
      next_server(&curserv, cursrvname, &curservport, NULL);
    }
  }

  end:

  if (datap)
    free(datap);

//  if (var->changed)
//    var->changed(var);
  return 1;
}


/* Returns the current string representation of the variable's value (if set)
 * otherwise, set's the variable to the value of the actual memory and returns that. 
 */
const char *var_string(variable_t *var)
{
  /* see if we already have the data ready to return */
  if (var->ldata)
    return var->ldata;
  else if (var->gdata)
    return var->gdata;

  /* otherwise, we'll parse/convert the memory associated with our variable */

  char *data = NULL;

  /* else: fill gdata and return it */
  if (var->flags & (VAR_INT|VAR_BOOL)) {
    /* only bother setting if we have a value that's not 0 */
    if (var->flags & VAR_DETECTED) {
      data = strdup(det_translate_num(*(int *) (var->mem)));
    } else {
      if (*(int *) (var->mem)) {
        data = (char *) my_calloc(1, 11);
        simple_snprintf(data, 11, "%d", *(int *) (var->mem));
      }
    }
  }
  else if (var->flags & VAR_STRING) {
    /* only bother setting if we have a non-empty string */
    if ((char *)var->mem && *(char *)var->mem)
      data = strdup((char *) var->mem);
  } else if (var->flags & VAR_RATE) {
    /* only bother setting if we don't have 0:0 */
    if ((*(rate_t *) (var->mem)).count && (*(rate_t *) (var->mem)).time) {
      data = (char *) my_calloc(1, 21);
      egg_snprintf(data, 21, "%d:%li", (*(rate_t *) (var->mem)).count, (*(rate_t *) (var->mem)).time);
    }
  } else if (var->flags & VAR_SERVERS) {
    /* only bother setting/checking if we have 'serverlist' alloc'd */
    if (*(struct server_list **)var->mem) {
      if (!use_server_type(var->name))
        return NULL;

      struct server_list *n = NULL;
      char list[2048] = "", buf[101] = "";

      for (n = (*(struct server_list **)var->mem); n; n = n->next) {
        if (n->port && n->port != default_port)
          simple_snprintf(buf, sizeof(buf), "%s:%d", n->name, n->port);
        else
          strlcat(buf, n->name, sizeof(buf));
        if (n->pass) {
           strlcat(buf, ":", sizeof(buf));
           strlcat(buf, n->pass, sizeof(buf));
        }
        strlcat(buf, ",", sizeof(buf));
        strlcat(list, buf, sizeof(list));
        buf[0] = 0;
      }
      list[strlen(list) - 1] = 0;
      data = strdup(list);
    }
  }

  if (data) {
    if (var->flags & VAR_SHUFFLE)
      shuffle(data, ",");

    if (!(var->flags & VAR_NODEF) && !var->gdata)
      var->gdata = data;
    else
      free(data);
  }

  return var->gdata ? var->gdata : NULL;
}

static variable_t *var_get_var_by_name(const char *name)
{
  int i = 0;

  for (i = 0; vars[i].name; i++)
    if (!egg_strcasecmp(vars[i].name, name))
      return &(vars[i]);
  return NULL;
}

void var_set(variable_t *var, const char *target, const char *datain)
{
  /* Don't set locally if the variable doesn't permit it. */
  if (target) {
    if (var->flags & VAR_NOLOC)
      return;

    struct userrec *botu = NULL;

    botu = get_user_by_handle(userlist, (char *) target);
    if (botu && (bot_hublevel(botu) != 999) && (var->flags & VAR_NOLHUB))
      return;
  }

  if (conf.bot->hub && (var->flags & VAR_NOGHUB))
    return;

  bool domem = 1, clear = 0;
//  bool freedata = 0;
  char *data = (char *) datain;

  if (!data || !strcmp(data, "-") || !data[0])
    clear = 1;

//  if (data && var->flags & VAR_SHUFFLE) {
//    char *datadup = strdup(data);

//    shuffle(datadup, ",");
//    data = datadup;
//    freedata++;
//  }

  if (target) {
    bool me = 0;

    if (!egg_strcasecmp(conf.bot->nick, target)) {
      me = 1;
      domem = 1;				/* always set the mem if it's local */
      if (var->ldata)
        free(var->ldata);

sdprintf("var: %s (local): %s", var->name, data);
      if (data && !clear)
        var->ldata = strdup(data);
      else
        var->ldata = NULL;
      /* if ldata is blank, see about setting the memory to the global setting... */
      if (domem && var->mem)
        var_set_mem(var, var->ldata ? var->ldata : var->gdata ? var->gdata : NULL);
    } 
  } else if (target == NULL) {
    if (var->ldata)
      domem = 0;
    if (var->gdata)
      free(var->gdata);
sdprintf("var: %s (global): %s", var->name, data);
    if (data && !clear)
      var->gdata = strdup(data);
    else {
      if (var->flags & VAR_CHANSET)
        var->gdata = strdup(def_chanset);
      else 
        var->gdata = NULL;
    }

    if (domem && var->mem)
      var_set_mem(var, var->gdata);

    /* It's global, we need to update the botnet with it */
    if (!set_noshare)
      botnet_send_var_broad(-1, var);
  }
//  if (freedata)
//    free(data);
}

void var_set_by_name(const char *target, const char *name, const char *data)
{
  variable_t *var = NULL;
  
  if ((var = var_get_var_by_name(name)))
    var_set(var, target, data);
}

void var_set_userentry(const char *target, const char *name, const char *data)
{
  struct userrec *u = NULL;
  bool me = 0;

  if (!egg_strcasecmp(conf.bot->nick, target))
    me = 1;

  if (me && conf.bot)
    u = conf.bot->u;
  else if (!me)
    u = get_user_by_handle(userlist, (char *) target);

  if (u) {
    struct xtra_key *xk = (struct xtra_key *) my_calloc(1, sizeof(struct xtra_key));

    xk->key = strdup(name);
    xk->data = data ? strdup(data) : NULL;
    set_user(&USERENTRY_SET, u, xk);                //This will send the change to the specified bot
  }
}

const char *var_get_str_by_name(const char *name)
{
  variable_t *var = NULL;

  if ((var = var_get_var_by_name(name)))
    return var_string(var);

  return NULL;
}

void *var_get_by_name(const char *name)
{
  variable_t *var = NULL;

  if ((var = var_get_var_by_name(name)))
    return var->mem;

  return NULL;
}

void init_vars()
{
  int i = 0;

  if (conf.bot->hub) {
    var_set_by_name(NULL, "servers6", "efnet.port80.se,irc.efnet.nl,irc.ipv6.homelien.no,efnet.ipv6.xs4all.nl,irc.ipv6.inter.net.il,irc.choopa.net,irc.ptptech.com");
    var_set_by_name(NULL, "servers", "irc.umich.edu,irc.kagmir.ca,irc.dataphone.se,irc.easynews.com,efnet.cs.hut.fi,irc.umn.edu,irc.blackened.com,irc.homelien.no,irc.blessed.net,irc.he.net,irc.inter.net.il,irc.du.se,irc.csbnet.se,efnet.xs4all.nl,irc.efnet.nl,irc.banetele.no,irc.daxnet.no,irc.inet.tele.dk,irc.dks.ca,irc.scnet.net,irc.arcti.ca,irc.avalonworks.ca,irc.foxlink.net,irc2.choopa.net,irc.dkom.at,efnet.demon.co.uk,irc.efnet.pl,irc.nac.net,irc.concentric.net,irc.choopa.net,irc.wh.verio.net,irc.mindspring.com,irc.desync.com,irc.mzima.net,irc.ptptech.com,efnet.port80.se,irc.pte.hu,irc.efnet.fr");
  }

  /* check mem for all vars and copy to our gdata */
  for (i = 0; vars[i].name; i++) {
    if (!vars[i].gdata && !vars[i].ldata) 
      var_string(&vars[i]);
  }
  var_set_by_name(NULL, "chanset", def_chanset);
  if (!strncmp(conf.bot->nick, "wtest", 5))
    var_set_by_name(NULL, "homechan", "#bryan");
}

/* This is used to parse (GLOBAL) userfile var lines and changes via .set from a remote hub */
void var_userfile_share_line(char *line, int idx, bool share)
{
  char *name = newsplit(&line);
  variable_t *var = NULL;

  if (!(var = var_get_var_by_name((char *) name))) {
     putlog(LOG_ERRORS, "*", "Unrecognized variable '%s' in userfile.", name);
     return;
  }

  set_noshare = 1;
  var_set(var, NULL, line);
  /* leaf bots don't need to bother attempting to share; there are no bots linked to us! */
  if (share && conf.bot->hub)
    botnet_send_var_broad(idx, var);
  set_noshare = 0;
}

static const char *var_get_bot_data(struct userrec *u, const char *name)
{
  if (!u)
    return NULL;

  struct xtra_key *xk = NULL;

  xk = (struct xtra_key *) get_user(&USERENTRY_SET, u);
  while (xk && strcmp(xk->key, name))
    xk = xk->next;

  return xk ? xk->data : NULL;
}


void var_parse_my_botset()
{
  int i;
  struct xtra_key *xk = NULL, *x = NULL;

  /* look for local vars inside our own USERENTRY_SET and set them in our cfg struct */
  set_noshare = 1;                      /* why bother sharing out our LOCAL settings? */
  xk = x = (struct xtra_key *) get_user(&USERENTRY_SET, conf.bot->u);
  for (i = 0; vars[i].name; i++) {
    xk = x;	/* reset pointer to beginning */

    while (xk && strcmp(xk->key, vars[i].name))
      xk = xk->next;
    if (xk && xk->key) {
      putlog(LOG_DEBUG, "*", "var_parse_my_botset: %s: %s", vars[i].name, xk->data);
      var_set(&vars[i], conf.bot->nick, xk->data);
      vars[i].flagged = 1;
    }
  }

  for (i = 0; vars[i].name; i++) {
    if (vars[i].ldata && !vars[i].flagged) {
      sdprintf("var[%s] nulled but we missed it!, reseting.", vars[i].name);
      var_set(&vars[i], conf.bot->nick, NULL);
    }
    vars[i].flagged = 0;
  }
  set_noshare = 0;
}

static void var_print_list(int idx, const char *name, const char *datain)
{
  if (!datain)
    return;

  char *word = NULL, *data = datain ? strdup(datain) : NULL, *datap = data;
  const char *delim = ",";
  int i = 1;
  
  dprintf(idx, "%s list:\n", name);

  while ((word = strsep(&data, delim))) {
    dprintf(idx, "  %d. %s\n", i, word);
    i++;
  }
  
  dprintf(idx, "End of %s list.\n", name);
  free(datap);
}

static int var_add_list(const char *botnick, variable_t *var, const char *element)
{
  if (!element)
    return 0;

  char *data = NULL, *olddata = NULL, *botdata = NULL;

  if (botnick) {                          //fetch data from bot's USERENTRY_SET
    botdata = (char *) var_get_bot_data(get_user_by_handle(userlist, (char *) botnick), var->name);
    olddata = botdata ? botdata : NULL;
  } else                                  //use global, no bot specified
    olddata = var->gdata ? var->gdata : NULL;

  /* Append to the olddata if there...*/
  size_t osiz = olddata ? strlen(olddata) : 0;

  if (olddata && osiz) {
    size_t esiz = strlen(element) + 1;		// element + ,

    data = (char *) my_calloc(1, osiz + esiz + 1);
    simple_snprintf(data, osiz + esiz + 1, "%s,%s", olddata, element);
  } else /* otherwise, just set the data to the element to be added */
    data = strdup(element);

  var_set(var, botnick, data);
  if (botnick)
    var_set_userentry(botnick, var->name, data);
  free(data);
  return 1;
}

static char *var_rem_list(const char *botnick, variable_t *var, const char *element)
{
  static char ret[101] = "";

  ret[0] = 0;

  if (!element)
    return ret;

  char *data = NULL, *olddata = NULL, *botdata = NULL, *olddatap = NULL, *olddatacp = NULL, *word = NULL;

  if (botnick) {                          //fetch data from bot's USERENTRY_SET
    botdata = (char *) var_get_bot_data(get_user_by_handle(userlist, (char *) botnick), var->name);
    olddatacp = botdata ? botdata : NULL;
  } else                                  //use global, no bot specified
    olddatacp = var->gdata ? var->gdata : NULL;

  if (!olddatacp)
    return 0;

  const char *delim = ",";
  int num = 0, i = 0;

  if (str_isdigit(element))
    num = atoi(element);

  olddata = olddatap = strdup(olddatacp);
  size_t tsiz = strlen(olddata) + 1;

  data = (char *) my_calloc(1, tsiz);

  while ((word = strsep(&olddata, delim))) {
    ++i;

    if ((num && num != i) || (!num && egg_strcasecmp(word, element))) {
      /* Reconstruct the left and right part of the list...*/
      if (data && data[0] && word && word[0])
        simple_snprintf(data, tsiz, "%s,%s", data, word);
      else
        simple_snprintf(data, tsiz, "%s", word);
    } else  /* minus the part we are removing ... */
      simple_snprintf(ret, sizeof(ret), "%s", word);
  }

  if (num <= i && ret[0]) {
    var_set(var, botnick, data);
    if (botnick)
      var_set_userentry(botnick, var->name, data);
  } else
    ret[0] = 0;
 
  free(data);
  free(olddatap);
  return ret;
}

bool write_vars_and_cmdpass(FILE *f, int idx)
{
  putlog(LOG_DEBUG, "@", "Writing set entries...");
  if (lfprintf(f, SET_NAME " - -\n") == EOF) /* Daemus */
      return 0;

  int i = 0;

  for (i = 0; vars[i].name; i++) {
    /* send blanks if our variable isn't set, theirs MIGHT be set and needs to be UNSET */
    if (lfprintf(f, "@ %s %s\n", vars[i].name, vars[i].gdata ? vars[i].gdata : "") == EOF)
      return 0;
  }

  for (struct cmd_pass *cp = cmdpass; cp; cp = cp->next)
    if (lfprintf(f, "- %s %s\n", cp->name, cp->pass) == EOF)
      return 0;

  return 1;
}


#define LIST_ADD  1
#define LIST_RM   2
#define LIST_SHOW 3
int cmd_set_real(const char *botnick, int idx, char *par)
{
  variable_t *var = NULL;
  char *name = NULL;
  const char *data = NULL, *botdata = NULL;
  int list = 0, i = 0;
  bool notyes = 1;

  if (par[0] && !egg_strncasecmp(par, "-yes", 4)) {
    notyes = 0;
    newsplit(&par);
  }

  if (botnick)
    putlog(LOG_CMDS, "*", "#%s# botset %s %s", dcc[idx].nick, botnick, par);
  else
    putlog(LOG_CMDS, "*", "#%s# set %s", dcc[idx].nick, par);

  if (par[0])
    name = newsplit(&par);

  if (name) {
    if (name[0] == '+')
      list = LIST_ADD; 
    else if (name[0] == '-')
      list = LIST_RM;
    else if (!egg_strcasecmp(name, "list"))
      list = LIST_SHOW;
  }

  if (list) {
    if (list != LIST_SHOW && name[1] && egg_strcasecmp(&name[1], "list"))
      name = &name[1];
    else {
      if (!par[0]) {
        dprintf(idx, "A variable must be specified!\n");
        return 0;
      }
      name = newsplit(&par);
    }
  }

  if (!list || (list && list != LIST_SHOW)) {
    if (par[0])
      data = (const char *) par;
    else if (!par[0] && list) {
      dprintf(idx, "A value must be specified!\n");
      return 0;
    }
  } else if (par[0] && list && list == LIST_SHOW) {
    dprintf(idx, "Data value ignored for listing.\n");
  }

  if (name && !(var = var_get_var_by_name(name))) {
    dprintf(idx, "No such variable: %s\n", name);
    return 0;
  }

  if (list && !(var->flags & VAR_LIST)) {
    dprintf(idx, "That variable is not a list!\n");
    return 0;
  }
  
  struct userrec *botu = NULL;
  bool ishub = 0;

  if (botnick) {
    botu = get_user_by_handle(userlist, (char *) botnick);
    if (data)
      dprintf(idx, "%-10s:\n", botnick);
    ishub = bot_hublevel(botu) == 999 ? 0 : 1;
  }


  if (!data) {
    while (vars[i].name) {
      botdata = NULL;
      if (!name)	//not looping all, provided with one...
        if (vars[i].name)
          var = &vars[i]; 
      
      if (!(var->flags & VAR_HIDE) && !((var->flags & VAR_PERM) && !isowner(dcc[idx].nick)) && 
          !(botnick && ((var->flags & VAR_NOLOC) || (ishub && (var->flags & VAR_NOLHUB))))
         ) {		//Just display the data
        if (botnick) {				//fetch data from bot's USERENTRY_SET
          botdata = var_get_bot_data(botu, var->name);
          data = botdata ? botdata : NULL;
        } else 					//use global, no bot specified
          data = var->gdata ? var->gdata : NULL;
       
        if (list && data)
          var_print_list(idx, var->name, data);
        else if (list && !data)
          dprintf(idx, "%s list not set.\n", var->name);
        else {
          char buf[51] = "";

          egg_snprintf(buf, sizeof(buf), "(%-6s)  %-19s:  ", var_type_name(var->flags), var->name);
//        dprintf(idx, "   %-15s:   %s\n", var->name, data);
          dumplots(idx, buf, data ? (char *) data : (char *) "(not set)");
        }
      }
      if (name)
        break;
      i++;
    }
  } else { // need to set it!
    if (!list && var->flags & VAR_LIST) {
      if (notyes) {
        dprintf(idx, "Using non-list functions on a list variable can be dangerous, please retype with -YES if you're sure:\n");
        if (botnick)
          dprintf(idx, "%sbotset %s -YES %s ...\n", settings.dcc_prefix, botnick, var->name);
        else
          dprintf(idx, "%sset -YES %s ...\n", settings.dcc_prefix, var->name);
        return 0;
      }
    }
    if (botnick) {
      if (var->flags & VAR_NOLOC) {
        dprintf(idx, "Sorry, cannot set '%s' locally.\n", var->name);
        return 0;
      }

      if (ishub && (var->flags & VAR_NOLHUB)) {
        dprintf(idx, "Sorry, cannot set '%s' locally for hubs.\n", var->name);
        return 0;
      }
    }

    if ((var->flags & VAR_PERM) && !isowner(dcc[idx].nick)) {
      dprintf(idx, "Sorry, only permanent owners may set '%s'.\n", var->name);
      return 0;
    }

    if (!strcmp(data, "-"))
      data = NULL;

    if (botnick) {
      char tmp[51] = "";
   
      if (list) 
        simple_snprintf(tmp, sizeof(tmp), "%s %s %s", var->name, list == LIST_ADD ? "+" : "-", data);
      else
        simple_snprintf(tmp, sizeof(tmp), "%s %s", var->name, data);

      update_mod((char *) botnick, dcc[idx].nick, "botset", tmp);
    }

    if (list) {
      if (list == LIST_ADD) {
        if (var_add_list(botnick, var, data)) {
          dprintf(idx, "Added '%s' to %s list.\n", data, var->name);
          return 1;
        }
      } else if (list == LIST_RM) {
        char *expanded_data = NULL;

        if ((expanded_data = var_rem_list(botnick, var, data)) && expanded_data[0]) {
          dprintf(idx, "Removed '%s' from %s list.\n", expanded_data, var->name);
          return 1;
        }
      }
      dprintf(idx, "Failed to modify %s list.\n", var->name);
      return 0;
    } else {
      var_set(var, botnick, data);
      if (botnick)
        var_set_userentry(botnick, name, data);

      dprintf(idx, "%s: %s\n", name, botnick ? (!data || (data[0] == '-') ? "(not set)" : data) : (var->gdata ? var->gdata : "(not set)"));
    }
    return 1;
  }
  return 0;
}
