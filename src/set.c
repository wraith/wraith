/*
 * set.c
 *
 */


#include "common.h"
#include "set.h"
#include "shell.h"
#include "botmsg.h"
#include "chanprog.h"
#include "match.h"
#include "main.h"
#include "misc.h"
#include "net.h"
#include "src/mod/server.mod/server.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/ctcp.mod/ctcp.h"
#include "users.h"
#include "userrec.h"
#include "userent.h"
#include "rfc1459.h"

#include "set_default.h"

static bool parsing_botset = 0;

char altchars[50] = "";
char alias[1024] = "";
bool auth_chan;
char auth_key[51] = "";
char auth_prefix[2] = "";
bool auth_obscure;
bool oidentd;
bool ident_botnick;
int dcc_autoaway;
bool irc_autoaway;
bool link_cleartext;
#ifdef NOT_USED
int badprocess = DET_IGNORE;
char process_list[1024] = "";
#endif
bool dccauth = 0;
char *def_chanset = "+enforcebans +dynamicbans +userbans -bitch +cycle -inactive +userexempts -dynamicexempts +userinvites -dynamicinvites -nodesynch -closed -take -voice -private -fastop +meankicks";
int cloak_script = 0;
rate_t close_threshold;
int fight_threshold;
int set_noshare = 0;
int fork_interval;
int hijack;
int in_bots;
int ison_time = 10;
int kill_threshold;
int lag_threshold;
int login;
char motd[512] = "";
char msgident[21] = "";
char msginvite[21] = "";
char msgop[21] = "";
char msgpass[21] = "";
int op_bots;
rate_t op_requests;
int promisc;
int trace;
bool manop_warn;
char homechan[51] = "";
char usermode[15] = "";

// VAR("bad-process",	&badprocess,		VAR_INT|VAR_DETECTED,				0, 4, "ignore"),
// VAR("process-list",	process_list,		VAR_STRING|VAR_LIST,				0, 0, NULL),
static variable_t vars[] = {
 VAR("alias", 		alias,			VAR_STRING|VAR_LIST|VAR_NOLOC|VAR_PERM,		0, 0, DEFAULT_ALIAS),
 VAR("altchars",	altchars,		VAR_STRING|VAR_NOLHUB,				0, 0, "-_\\`^[]"),
 VAR("auth-chan",	&auth_chan,		VAR_INT|VAR_BOOL|VAR_NOLHUB,			0, 1, "1"),
 VAR("auth-key",	auth_key,		VAR_STRING|VAR_PERM,				0, 0, NULL),
 VAR("auth-obscure",	&auth_obscure,		VAR_INT|VAR_BOOL,				0, 1, "0"),
 VAR("auth-prefix",	auth_prefix,		VAR_STRING|VAR_NOLHUB|VAR_PERM,			0, 0, "+"),
 VAR("chanset",		glob_chanset,		VAR_STRING|VAR_CHANSET|VAR_NOLHUB,		0, 0, NULL),
 VAR("cloak-script",	&cloak_script,		VAR_INT|VAR_CLOAK|VAR_NOLHUB,			0, 10, "0"),
 VAR("close-threshold",	&close_threshold,	VAR_RATE|VAR_NOLOC,				0, 0, "0:0"),
 VAR("dcc-autoaway",	&dcc_autoaway,		VAR_INT|VAR_NOLOC,				0, (5*60*60), "1800"),
 VAR("dccauth",		&dccauth,		VAR_INT|VAR_BOOL,				0, 1, "0"),
 VAR("fight-threshold",	&fight_threshold,	VAR_INT|VAR_NOLOC,				0, 0, "0"),
 VAR("flood-ctcp",	&flood_ctcp,		VAR_RATE|VAR_NOLHUB,				0, 0, "3:60"),
 VAR("flood-g",		&flood_g,		VAR_RATE|VAR_NOLHUB,				0, 0, "6:2"),
 VAR("flood-msg",	&flood_msg,		VAR_RATE|VAR_NOLHUB,				0, 0, "5:60"),
 VAR("fork-interval",	&fork_interval,		VAR_INT,					10, 0, "0"),
 VAR("hijack",		&hijack,		VAR_INT|VAR_DETECTED|VAR_PERM,			0, 4, "die"),
 VAR("homechan",	homechan,		VAR_STRING|VAR_NOLOC|VAR_HIDE,			0, 0, NULL),
 VAR("ident-botnick",   &ident_botnick,		VAR_INT|VAR_BOOL|VAR_NOLHUB,			0, 1, "0"),
 VAR("in-bots",		&in_bots,		VAR_INT|VAR_NOLOC,				1, MAX_BOTS, "2"),
 VAR("irc-autoaway",	&irc_autoaway,		VAR_INT|VAR_NOLHUB|VAR_BOOL,			0, 1, "1"),
 VAR("jupenick",	jupenick,		VAR_STRING|VAR_NOHUB|VAR_JUPENICK|VAR_NODEF,  	0, 0, NULL),
 VAR("kill-threshold",	&kill_threshold,	VAR_INT|VAR_NOLOC,				0, 0, "0"),
 VAR("lag-threshold",	&lag_threshold,		VAR_INT|VAR_NOLHUB,				0, 0, "15"),
 VAR("link_cleartext",	&link_cleartext,	VAR_INT|VAR_NOLOC|VAR_BOOL,			0, 1, "0"),
 VAR("login",		&login,			VAR_INT|VAR_DETECTED,				0, 4, "warn"),
 VAR("manop-warn",	&manop_warn,		VAR_INT|VAR_BOOL|VAR_NOLHUB,			0, 1, "1"),
 VAR("motd",		motd,			VAR_STRING|VAR_HIDE|VAR_NOLOC,			0, 0, NULL),
 VAR("msg-ident",	msgident,		VAR_STRING|VAR_NOLHUB,				0, 0, NULL),
 VAR("msg-invite",	msginvite,		VAR_STRING|VAR_NOLHUB,				0, 0, NULL),
 VAR("msg-op",		msgop,			VAR_STRING|VAR_NOLHUB,				0, 0, NULL),
 VAR("msg-pass",	msgpass,		VAR_STRING|VAR_NOLHUB,				0, 0, NULL),
 VAR("nick",		origbotname,		VAR_STRING|VAR_NOHUB|VAR_NICK|VAR_NODEF,	0, 0, NULL),
 VAR("notify-time",	&ison_time,		VAR_INT|VAR_NOLHUB,				1, 30, "10"),
 VAR("oidentd",		&oidentd,		VAR_INT|VAR_BOOL|VAR_NOLHUB,			0, 1, "0"),
 VAR("op-bots",		&op_bots,		VAR_INT|VAR_NOLOC,				1, MAX_BOTS, "1"),
 VAR("op-requests",	&op_requests,		VAR_RATE|VAR_NOLOC,				0, 0, "2:5"),
 VAR("promisc",		&promisc,		VAR_INT|VAR_DETECTED,				0, 4, "warn"),
 VAR("realname",	botrealname,		VAR_STRING|VAR_NOLHUB,				0, 0, "* I'm too lame to read BitchX.doc *"),
 VAR("server-port",	&default_port,		VAR_INT|VAR_SHORT|VAR_NOLHUB,			0, 65535, "6667"),
 VAR("servers",		&serverlist,		VAR_SERVERS|VAR_LIST|VAR_SHUFFLE|VAR_NOLHUB|VAR_NOLDEF,	0, 0, DEFAULT_SERVERS),
 VAR("servers6",	&serverlist,		VAR_SERVERS|VAR_LIST|VAR_SHUFFLE|VAR_NOLHUB|VAR_NOLDEF,	0, 0, DEFAULT_SERVERS6),
 VAR("trace",		&trace,			VAR_INT|VAR_DETECTED,				0, 4, "die"),
 VAR("usermode",	&usermode,		VAR_STRING|VAR_NOLHUB,				0, 0, "+iws"),
 VAR(NULL,		NULL,			0,						0, 0, NULL)
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

#ifdef NOT_USED
const char *var_find_by_mem(void *mem)
{
  int i = 0;

  for (i = 0; vars[i].name; i++) {
    if (vars[i].mem == mem)
      return vars[i].name;
  }
  return "";  
}
#endif

/* sanitize the variable data string */
char *var_sanitize(variable_t *var, const char *data)
{
/* var->a var->b */
  char *dataout = NULL;

  if ((var->flags & VAR_INT) && !(var->flags & VAR_BOOL) && !(var->flags & VAR_DETECTED)) {
    bool isnumber = 0;
    int number = 0;

    isnumber = data ? str_isdigit(data) : 0;
    if (isnumber)
      number = atoi(data);

    /* Do limit enforcing... */
    if (number < var->a)
      number = var->a;
    else if (var->b && (number > var->b))
      number = var->b;

    dataout = (char*) my_calloc(1, 11);
    simple_snprintf(dataout, 11, "%d", number);
  } else if (var->flags & VAR_DETECTED) {
    if (data) {
      if (str_isdigit(data)) {
        int number = atoi(data);
        dataout = strdup(det_translate_num(number));
      } else if (!egg_strcasecmp(data, "ignore") || !egg_strcasecmp(data, "warn") ||
                 !egg_strcasecmp(data, "die") || !egg_strcasecmp(data, "reject") ||
                 !egg_strcasecmp(data, "suicide")) {
        dataout = strdup(data);
      }
    } else
      dataout = strdup("ignore");
  } else if (var->flags & VAR_BOOL) {
    int num = 0;

    if (data && (str_isdigit(data) || 
                 !egg_strcasecmp(data, "true") || 
                 !egg_strcasecmp(data, "on") || 
                 !egg_strcasecmp(data, "off") || 
                 !egg_strcasecmp(data, "false"))) {
      if (str_isdigit(data))
        num = atoi(data);

      if (num > 0 || !egg_strcasecmp(data, "true") || !egg_strcasecmp(data, "on"))
        num = 1;
      else if (num < 0 || !egg_strcasecmp(data, "false") || !egg_strcasecmp(data, "off"))
        num = 0;
    }
    dataout = (char*) my_calloc(1, 2);
    simple_snprintf(dataout, 2, "%u", num ? 1 : 0);
  } else if (var->flags & VAR_STRING) {
    dataout = data ? strdup(data) : NULL;
  } else if (var->flags & VAR_RATE) {
    char *p = NULL;
    rate_t rate = {0, 0};
    
    if (data && (p = strchr(data, ':'))) {
      *p = 0;
      p++;

      if (str_isdigit(data))
        rate.count = atoi(data);
      if (str_isdigit(p))
        rate.time = atoi(p);
      p--;
      *p = ':';
    }

    /* No limit enforcing yet */
    dataout = (char*) my_calloc(1, 21);
    simple_snprintf(dataout, 21, "%d:%d", rate.count, rate.time);
  } else if ((var->flags & VAR_SERVERS)) {
    dataout = data ? strdup(data) : NULL;
  }

  return dataout;
}

/* set the ptr to the new data */
static bool var_set_mem(variable_t *var, const char *datain)
{
  char *data = (datain && datain[0]) ? strdup(datain) : NULL, *datap = data;
#ifdef DEBUG
sdprintf("var (mem): %s -> %s", var->name, datain ? datain : "(NULL)");
#endif

  if (data && var->flags & VAR_SHUFFLE) {
//    char *datadup = strdup(data);

//    shuffle(datadup, ",");
//    data = datadup;
//    freedata++;
    shuffle(data, ",", strlen(data) + 1);
  }

  /* figure out it's type and set it's variable to the data */
  if ((var->flags & VAR_INT) && !(var->flags & VAR_BOOL) && !(var->flags & VAR_DETECTED)) {
    int number = atoi(data);

    if (var->flags & VAR_CLOAK && !conf.bot->hub) {
      if (number == 0)
        number = randint(CLOAK_COUNT - 1) + 1;
    }

    if (var->flags & VAR_SHORT)
      *(short *) (var->mem) = (short) number;
    else
      *(int *) (var->mem) = number;

    if (var->flags & VAR_CLOAK && !conf.bot->hub)
      scriptchanged();
  } else if (var->flags & VAR_DETECTED) {
    int number = data ? det_translate(data) : DET_IGNORE;
#ifndef DEBUG
    if (number < 2 && !egg_strcasecmp(var->name, "trace"))
      number = DET_DIE;
#endif
    *(int *) (var->mem) = number;
  } else if (var->flags & VAR_BOOL) {
    bool num = 0;
    if (data[0] == '0')
      num = 0;
    else if (data[0] == '1')
      num = 1;

    *(bool *) (var->mem) = num;

    if (!strcmp(var->name, "ident-botnick"))
      strlcpy(botuser, conf.username && !num ? conf.username : origbotname, 21);
  } else if (var->flags & VAR_STRING) {
    if (data)
      strlcpy((char *) var->mem, data, var->size);
    else
      ((char *) var->mem)[0] = 0;

    if (!conf.bot->hub) {
      if (var->flags & VAR_JUPENICK) {
        //rolls = 0;
        //altnick_char = 0;
        // Don't send nick changes on restart, no need.
        if (server_online && !parsing_botset) {
          if (data) {
            // If not on the new nick, jump to it
            if (rfc_casecmp(botname, (char *) var->mem)) {
              tried_jupenick = 1;
              dprintf(DP_SERVER, "NICK %s\n", (char *) var->mem);
            }
          } else {
            // Unset jupenick, try for 'nick' now
            if (rfc_casecmp(botname, origbotname))
              dprintf(DP_SERVER, "NICK %s\n", origbotname);
          }
        }
      } else if (var->flags & VAR_NICK) {
        // Default to botnick
        if (!data)
          strlcpy((char *) var->mem, conf.bot->nick, var->size);
        // Don't send nick changes on restart, no need.
        if (server_online && !parsing_botset) {
          // If not already on jupenick and not on the new nick, jump to the new nick
          if (jupenick[0] && rfc_casecmp(botname, jupenick) && rfc_casecmp(botname, (char *) var->mem))
            dprintf(DP_SERVER, "NICK %s\n", (char *) var->mem);
          // No jupenick set
          else if (!jupenick[0] && rfc_casecmp(botname, (char *) var->mem))
            dprintf(DP_SERVER, "NICK %s\n", (char *) var->mem);
        }
      }
    }
  } else if (var->flags & VAR_RATE) {
    char *p = NULL;
    
    if (data && (p = strchr(data, ':'))) {
      *p = 0;
      p++;
 
      (*(rate_t *) (var->mem)).count = atoi(data);
      (*(rate_t *) (var->mem)).time = atoi(p);
      --p;
      *p = ':';
    }
  } else if ((var->flags & VAR_SERVERS) && use_server_type(var->name)) {
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
  if ((var->flags & VAR_INT) && !(var->flags & VAR_BOOL)) {
    /* only bother setting if we have a value that's not 0 */
    if (var->flags & VAR_DETECTED) {
      data = strdup(det_translate_num(*(int *) (var->mem)));
    } else {
      if (*(int *) (var->mem)) {
        data = (char *) my_calloc(1, 11);
        simple_snprintf(data, 11, "%d", *(int *) (var->mem));
      }
    }
  } else if (var->flags & (VAR_BOOL)) {
    /* only bother setting if we have a value that's not 0 */
    if (*(int *) (var->mem)) {
      bool num = *(bool *) (var->mem);
      data = (char *) my_calloc(1, 2);
      /* Only actually set 0 or 1 */
      simple_snprintf(data, 2, "%d", num ? 1 : 0);
    }
  } else if (var->flags & VAR_STRING) {
    /* only bother setting if we have a non-empty string */
    if ((char *)var->mem && *(char *)var->mem)
      data = strdup((char *) var->mem);
  } else if (var->flags & VAR_RATE) {
    /* only bother setting if we don't have 0:0 */
    if ((*(rate_t *) (var->mem)).count && (*(rate_t *) (var->mem)).time) {
      data = (char *) my_calloc(1, 21);
      simple_snprintf(data, 21, "%d:%d", (*(rate_t *) (var->mem)).count, (*(rate_t *) (var->mem)).time);
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
      shuffle(data, ",", strlen(data) + 1);

    if ((var->flags & VAR_NODEF) && !var->gdata) {
       free(data);
       data = NULL;
    }
  }

  return data;
}

static int comp_variable_t(const void *m1, const void *m2) {
  const variable_t *mi1 = (const variable_t *) m1;
  const variable_t *mi2 = (const variable_t *) m2;
  return egg_strcasecmp(mi1->name, mi2->name);
}

static inline variable_t *var_get_var_by_name(const char *name)
{
  variable_t key;
  key.name = name;
  return (variable_t*) bsearch(&key, &vars, sizeof(vars)/sizeof(variable_t), sizeof(variable_t), comp_variable_t);
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

  /* If no data or data is not -
     sanitize the data */
  char *sdata = NULL;
  /* Make a temporary to free at the end */
  if (data && strcmp(data, "-") && (!var->def || (var->def && strcmp(data, var->def)))) {
    sdata = var_sanitize(var, data);
    data = sdata;
  }

  /* If no data, or data is "-", do a clear */
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

sdprintf("var: %s (local): %s", var->name, data ? data : "(NULL)");
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
sdprintf("var: %s (global): %s", var->name, data ? data : "(NULL)");
    if (data && !clear)
      var->gdata = strdup(data);
    else {
      if (var->flags & VAR_CHANSET)
        var->gdata = strdup(def_chanset);
      else 
        var->gdata = var->def ? strdup(var->def) : NULL;
    }

    if (domem && var->mem)
      var_set_mem(var, var->gdata);

    /* It's global, we need to update the botnet with it */
    if (!set_noshare)
      botnet_send_var_broad(-1, var);
  }
//  if (freedata)
//    free(data);
  if (sdata)
    free(sdata);
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

  /* initialize vars: defaults -> gdata */
  for (i = 0; vars[i].name; i++) {
    if (!vars[i].gdata && !vars[i].ldata && !(!conf.bot->hub && (vars[i].flags & VAR_NOLDEF)))
      var_set(&vars[i], NULL, NULL);		//empty out and set to defaults
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
  parsing_botset = 1;
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
  parsing_botset = 0;
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

static bool var_find_list(const char *botnick, variable_t *var, const char *element) {
  char *olddata = NULL;

  if (botnick) {                          //fetch data from bot's USERENTRY_SET
    char *botdata = (char *) var_get_bot_data(get_user_by_handle(userlist, (char *) botnick), var->name);
    olddata = botdata ? botdata : NULL;
  } else                                  //use global, no bot specified
    olddata = var->gdata ? var->gdata : NULL;

  if (!olddata)
    return false;

  char *item = NULL, *data = strdup(olddata), *datap = data;
  const char *delim = ",";
  size_t slen = 0;
  char *p = NULL;

  /* The first word only .. */
  if (!strcmp(var->name, "alias") && (p = strchr(element, ' ')))
    slen = p - element;
  /* Or the whole word */
  else
    slen = strlen(element);
  
  while ((item = strsep(&data, delim)))
    if (!egg_strncasecmp(item, element, slen)) {
      free(datap);
      return true;
    }
  free(datap);
  return false;
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

  char *olddata = NULL, *botdata = NULL, *olddatap = NULL, *olddatacp = NULL;

  if (botnick) {                          //fetch data from bot's USERENTRY_SET
    botdata = (char *) var_get_bot_data(get_user_by_handle(userlist, (char *) botnick), var->name);
    olddatacp = botdata ? botdata : NULL;
  } else                                  //use global, no bot specified
    olddatacp = var->gdata ? var->gdata : NULL;

  if (!olddatacp)
    return 0;

  const char *delim = ",";
  int num = 0, i = 0;
  size_t elen = 0;

  if (str_isdigit(element))
    num = atoi(element);
  else
    elen = strlen(element);

  olddata = olddatap = strdup(olddatacp);
  size_t tsiz = strlen(olddata) + 1;

  char *data = (char *) my_calloc(1, tsiz);
  char *word = NULL;
  while ((word = strsep(&olddata, delim))) {
    ++i;

    if ((num && num != i) || (!num && egg_strncasecmp(word, element, elen))) {
      /* Reconstruct the left and right part of the list...*/
      if (data && data[0] && word && word[0]) {
        strlcat(data, ",", tsiz);
        strlcat(data, word, tsiz);
      } else
        strlcpy(data, word, tsiz);
    } else  /* minus the part we are removing ... */
      strlcpy(ret, word, sizeof(ret));
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
  bool notyes = 1, wildcard = 0;

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
  
  if (name && strchr(name, '*'))
    wildcard = 1;

  if (wildcard && par[0]) {
    dprintf(idx, "Wildcards may only be used for listing matching variables.\n");
    return 0;
  } else if (!wildcard) {
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
      if (!name || wildcard)	//not looping all, provided with one...
        if (vars[i].name)
          var = &vars[i]; 

      if (wildcard && !wild_match(name, var->name)) {
        ++i;
        continue;
      }
      
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
      if (name && !wildcard)
        break;
      ++i;
    }
  } else { // need to set it!
    if (!list && var->flags & VAR_LIST) {
      if (notyes) {
        dprintf(idx, "Using non-list functions on a list variable can be dangerous, please retype with -YES if you're sure:\n");
        if (botnick)
          dprintf(idx, "%sbotset %s -YES %s ...\n", (dcc[idx].u.chat->channel >= 0) ? settings.dcc_prefix : "", botnick, var->name);
        else
          dprintf(idx, "%sset -YES %s ...\n", (dcc[idx].u.chat->channel >= 0) ? settings.dcc_prefix : "", var->name);
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
      if (strchr(data, ',')) {
        dprintf(idx, "The ',' character may not be used with set list functions.\n");
        return 0;
      }
      if (list == LIST_ADD) {
        if (var_find_list(botnick, var, data)) {
          if (!strcmp(var->name, "alias")) {
            char *p = strchr(data, ' ');
            if (p)
              *p = 0;
          }
          dprintf(idx, "Item '%s' is already in the %s list.\n", data, var->name);
          return 0;
        } else if (var_add_list(botnick, var, data)) {
          dprintf(idx, "Added '%s' to %s list.\n", data, var->name);
          return 1;
        }
      } else if (list == LIST_RM) {
        char *expanded_data = NULL;

        if ((expanded_data = var_rem_list(botnick, var, data)) && expanded_data[0]) {
          dprintf(idx, "Removed '%s' from %s list.\n", expanded_data, var->name);
          return 1;
        } else if (!var_find_list(botnick, var, data)) {
          if (!strcmp(var->name, "alias")) {
            char *p = strchr(data, ' ');
            if (p)
              *p = 0;
          }
          dprintf(idx, "Item '%s' does not exist in the %s list.\n", data, var->name);
          return 0;
        }
      }
      dprintf(idx, "Failed to modify %s list.\n", var->name);
      return 0;
    } else {
      var_set(var, botnick, data);
      if (botnick)
        var_set_userentry(botnick, name, data);

      dprintf(idx, "%s: %s\n", name, botnick ? (!data || (data[0] == '-' && !data[1]) ? "(not set)" : data) : (var->gdata ? var->gdata : "(not set)"));
    }
    return 1;
  }
  return 0;
}
