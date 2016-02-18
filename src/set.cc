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
#include "src/mod/irc.mod/irc.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/ctcp.mod/ctcp.h"
#include "users.h"
#include "userrec.h"
#include "userent.h"
#include "rfc1459.h"
#include <bdlib/src/Stream.h>
#include <bdlib/src/String.h>

#include "set_default.h"

static bool parsing_botset = 0;

char altchars[50] = "";
char alias[1024] = "";
char rbl_servers[1024] = "";
char groups[1024] = "";
bool auth_chan;
char auth_key[51] = "";
char auth_prefix[2] = "";
bool auth_obscure;
bool oidentd;
bool ident_botnick;
int dcc_autoaway;
bool irc_autoaway;
bool link_cleartext;
bool dccauth = 0;
bool use_deaf = 0;
bool use_callerid = 0;
int cloak_script = 0;
rate_t close_threshold;
int fight_threshold;
int set_noshare = 0;
int hijack;
int in_bots;
int ison_time = 10;
int kill_threshold;
int lag_threshold;
int login;
/* Number of seconds to wait between transmitting queued lines to the server
 * lower this value at your own risk.  ircd is known to start flood control
 * at 512 bytes/2 seconds.
 */
int msgburst;
int msgrate;
char motd[512] = "";
char msgident[21] = "";
char msginvite[21] = "";
char msgop[21] = "";
char msgpass[21] = "";
char msgrelease[21] = "";
int op_bots;
rate_t op_requests;
int promisc;
int trace;
bool manop_warn;
char homechan[51] = "";
char usermode[15] = "";
bool fish_auto_keyx = 0;
bool fish_paranoid = 0;
int server_cycle_wait;
int wait_split;

////// THIS MUST REMAIN SORTED: !LC_ALL=C sort
// VAR("bad-process",	&badprocess,		VAR_INT|VAR_DETECTED,				0, 4, "ignore"),
// VAR("process-list",	process_list,		VAR_STRING|VAR_LIST,				0, 0, NULL),
static variable_t vars[] = {
 VAR("alias", 		alias,			VAR_STRING|VAR_LIST|VAR_NOLOC|VAR_PERM,		0, 0, DEFAULT_ALIAS),
 VAR("altchars",	altchars,		VAR_WORD|VAR_NOLHUB,				0, 0, "-_\\`^[]"),
 VAR("auth-chan",	&auth_chan,		VAR_INT|VAR_BOOL|VAR_NOLHUB,			0, 1, "1"),
 VAR("auth-key",	auth_key,		VAR_STRING|VAR_PERM,				0, 0, NULL),
 VAR("auth-obscure",	&auth_obscure,		VAR_INT|VAR_BOOL,				0, 1, "0"),
 VAR("auth-prefix",	auth_prefix,		VAR_WORD|VAR_NOLHUB|VAR_PERM,			0, 0, "+"),
 VAR("callerid",	&use_callerid,		VAR_INT|VAR_BOOL|VAR_NOLHUB,			0, 1, "1"),
 VAR("cloak-script",	&cloak_script,		VAR_INT|VAR_CLOAK|VAR_NOLHUB,			0, 10, "0"),
 VAR("close-threshold",	&close_threshold,	VAR_RATE|VAR_NOLOC,				0, 0, "0:0"),
 VAR("dcc-autoaway",	&dcc_autoaway,		VAR_INT|VAR_NOLOC,				0, (5*60*60), "1800"),
 VAR("dccauth",		&dccauth,		VAR_INT|VAR_BOOL,				0, 1, "0"),
 VAR("deaf",		&use_deaf,		VAR_INT|VAR_BOOL|VAR_NOLHUB,			0, 1, "1"),
 VAR("fight-threshold",	&fight_threshold,	VAR_INT|VAR_NOLOC,				0, 0, "0"),
 VAR("fish-auto-keyx",	&fish_auto_keyx,	VAR_INT|VAR_BOOL|VAR_NOLHUB,			0, 1, "1"),
 VAR("fish-paranoid",	&fish_paranoid,		VAR_INT|VAR_BOOL|VAR_NOLHUB,			0, 1, "0"),
 VAR("flood-callerid",	&flood_callerid,	VAR_RATE|VAR_NOLHUB,				0, 0, "6:2"),
 VAR("flood-ctcp",	&flood_ctcp,		VAR_RATE|VAR_NOLHUB,				0, 0, "3:60"),
 VAR("flood-msg",	&flood_msg,		VAR_RATE|VAR_NOLHUB,				0, 0, "5:60"),
 VAR("groups",		groups,			VAR_STRING|VAR_LIST|VAR_NOLHUB,			0, 0, "main"),
 VAR("hijack",		&hijack,		VAR_INT|VAR_DETECTED|VAR_PERM,			0, 4, "die"),
 VAR("homechan",	homechan,		VAR_WORD|VAR_NOLOC|VAR_HIDE,			0, 0, NULL),
 VAR("ident-botnick",   &ident_botnick,		VAR_INT|VAR_BOOL|VAR_NOLHUB,			0, 1, "0"),
 VAR("in-bots",		&in_bots,		VAR_INT|VAR_NOLOC,				1, MAX_BOTS, "2"),
 VAR("irc-autoaway",	&irc_autoaway,		VAR_INT|VAR_NOLHUB|VAR_BOOL,			0, 1, "1"),
 VAR("jupenick",	jupenick,		VAR_WORD|VAR_NOHUB|VAR_JUPENICK|VAR_NODEF,  	0, 0, NULL),
 VAR("kill-threshold",	&kill_threshold,	VAR_INT|VAR_NOLOC,				0, 0, "0"),
 VAR("lag-threshold",	&lag_threshold,		VAR_INT|VAR_NOLHUB,				0, 0, "15"),
 VAR("link_cleartext",	&link_cleartext,	VAR_INT|VAR_NOLOC|VAR_BOOL,			0, 1, "0"),
 VAR("login",		&login,			VAR_INT|VAR_DETECTED,				0, 4, "warn"),
 VAR("manop-warn",	&manop_warn,		VAR_INT|VAR_BOOL|VAR_NOLHUB,			0, 1, "1"),
 VAR("motd",		motd,			VAR_STRING|VAR_HIDE|VAR_NOLOC,			0, 0, NULL),
 VAR("msg-ident",	msgident,		VAR_WORD|VAR_NOLHUB,				0, 0, NULL),
 VAR("msg-invite",	msginvite,		VAR_WORD|VAR_NOLHUB,				0, 0, NULL),
 VAR("msg-op",		msgop,			VAR_WORD|VAR_NOLHUB,				0, 0, NULL),
 VAR("msg-pass",	msgpass,		VAR_WORD|VAR_NOLHUB,				0, 0, NULL),
 VAR("msg-release",	msgrelease,		VAR_WORD|VAR_NOLHUB,				0, 0, NULL),
 VAR("msgburst",	&msgburst,		VAR_INT|VAR_NOLHUB,				1, 90, "5"),
 VAR("msgrate",		&msgrate,		VAR_INT|VAR_NOLHUB,				DEQ_RATE, 10000, "2200"),
 VAR("nick",		origbotname,		VAR_WORD|VAR_NOHUB|VAR_NICK|VAR_NODEF,	0, 0, NULL),
 VAR("notify-time",	&ison_time,		VAR_INT|VAR_NOLHUB,				1, 30, "10"),
 VAR("oidentd",		&oidentd,		VAR_INT|VAR_BOOL|VAR_NOLHUB,			0, 1, "0"),
 VAR("op-bots",		&op_bots,		VAR_INT|VAR_NOLOC,				1, MAX_BOTS, "1"),
 VAR("op-requests",	&op_requests,		VAR_RATE|VAR_NOLOC,				0, 0, "2:5"),
 VAR("promisc",		&promisc,		VAR_INT|VAR_DETECTED,				0, 4, "ignore"),
 VAR("rbl-servers",	rbl_servers,		VAR_STRING|VAR_LIST|VAR_SHUFFLE|VAR_NOLHUB,	0, 0, DEFAULT_RBL),
 VAR("realname",	botrealname,		VAR_STRING|VAR_NOLHUB,				0, 0, "* I'm too lame to read BitchX.doc *"),
 VAR("server-cycle-wait",&server_cycle_wait,	VAR_INT|VAR_NOLHUB,				5, 500, "30"),
 VAR("server-port",	&default_port,		VAR_INT|VAR_SHORT|VAR_NOLHUB,			0, 65535, "6667"),
 VAR("server-port-ssl",	&default_port_ssl,	VAR_INT|VAR_SHORT|VAR_NOLHUB,			0, 65535, "6697"),
 VAR("server-use-ssl",	&ssl_use,		VAR_INT|VAR_BOOL|VAR_NOLHUB,			0, 1, "0"),
 VAR("servers",		&serverlist,		VAR_SERVERS|VAR_LIST|VAR_SHUFFLE|VAR_NOLHUB|VAR_NOLDEF,	0, 0, DEFAULT_SERVERS),
 VAR("servers-ssl",	&serverlist,		VAR_SERVERS|VAR_LIST|VAR_SHUFFLE|VAR_NOLHUB|VAR_NOLDEF,	0, 0, DEFAULT_SERVERS_SSL),
 VAR("servers6",	&serverlist,		VAR_SERVERS|VAR_LIST|VAR_SHUFFLE|VAR_NOLHUB|VAR_NOLDEF,	0, 0, DEFAULT_SERVERS6),
 VAR("servers6-ssl",	&serverlist,		VAR_SERVERS|VAR_LIST|VAR_SHUFFLE|VAR_NOLHUB|VAR_NOLDEF,	0, 0, DEFAULT_SERVERS6_SSL),
 VAR("trace",		&trace,			VAR_INT|VAR_DETECTED,				0, 4, "die"),
 VAR("usermode",	&usermode,		VAR_WORD|VAR_NOLHUB,				0, 0, "+iws"),
 VAR("wait-split",	&wait_split,		VAR_INT|VAR_NOLHUB,				0, 86400, "1000"),
 VAR(NULL,		NULL,			0,						0, 0, NULL)
};


static inline variable_t *var_get_var_by_name(const char *name);

static const char* get_server_type()
{
  if (!ssl_use && !conf.bot->net.host6 && !conf.bot->net.ip6) {
    return "servers";
  } else if (!ssl_use && (conf.bot->net.host6 || conf.bot->net.ip6)) {
    return "servers6";
  } else if (ssl_use && !conf.bot->net.host6 && !conf.bot->net.ip6) {
    return "servers-ssl";
  } else if (ssl_use && (conf.bot->net.host6 || conf.bot->net.ip6)) {
    return "servers6-ssl";
  }
  return "";
}

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

    dataout = (char*) calloc(1, 11);
    simple_snprintf(dataout, 11, "%d", number);
  } else if (var->flags & VAR_DETECTED) {
    if (data) {
      if (str_isdigit(data)) {
        int number = atoi(data);
        dataout = strdup(det_translate_num(number));
      } else if (!strcasecmp(data, "ignore") || !strcasecmp(data, "warn") ||
                 !strcasecmp(data, "die") || !strcasecmp(data, "reject") ||
                 !strcasecmp(data, "suicide")) {
        dataout = strdup(data);
      }
    } else
      dataout = strdup("ignore");
  } else if (var->flags & VAR_BOOL) {
    int num = 0;

    if (data && (str_isdigit(data) || 
                 !strcasecmp(data, "true") ||
                 !strcasecmp(data, "on") ||
                 !strcasecmp(data, "off") ||
                 !strcasecmp(data, "false"))) {
      if (str_isdigit(data))
        num = atoi(data);

      if (num > 0 || !strcasecmp(data, "true") || !strcasecmp(data, "on"))
        num = 1;
      else if (num < 0 || !strcasecmp(data, "false") || !strcasecmp(data, "off"))
        num = 0;
    }
    dataout = (char*) calloc(1, 2);
    simple_snprintf(dataout, 2, "%u", num ? 1 : 0);
  } else if (var->flags & VAR_STRING) {
    dataout = data ? strdup(data) : NULL;
  } else if (var->flags & VAR_WORD) {
    if (data) {
      const char *p = strchr(data, ' ');
      if (!p)
        dataout = strdup(data);
      else
        dataout = strldup(data, p - data);
    } else {
      data = NULL;
    }


  } else if (var->flags & VAR_RATE) {
    const char *p = NULL;
    rate_t rate = {0, 0};
    
    if (data && (p = strchr(data, ':'))) {
      char *p_count = strldup(data, p - data);

      if (str_isdigit(p_count))
        rate.count = atoi(p_count);
      free(p_count);
      if (str_isdigit(p + 1))
        rate.time = atoi(p + 1);
    }

    /* No limit enforcing yet */
    dataout = (char*) calloc(1, 21);
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
    if (number < 2 && !strcasecmp(var->name, "trace"))
      number = DET_DIE;
#endif
    *(int *) (var->mem) = number;
  } else if (var->flags & VAR_BOOL) {
    bool olddata = *(bool*)(var->mem);
    bool num = 0;
    if (data == NULL || data[0] == '0')
      num = 0;
    else if (data[0] == '1')
      num = 1;

    *(bool *) (var->mem) = num;

    if (!strcmp(var->name, "ident-botnick"))
      strlcpy(botuser, conf.username && !num ? conf.username : origbotname, 21);
    else if ((!strcmp(var->name, "deaf") && deaf_char) || (!strcmp(var->name, "callerid") && callerid_char)) {
      if (server_online) {
        char which = 0;
        if (num == 1 && olddata == 0)
          which = '+';
        else if (num == 0 && olddata == 1)
          which = '-';
        if (which) {
          char mode_char = strcmp(var->name, "deaf") == 0 ? deaf_char : callerid_char;
          if ((mode_char == deaf_char && in_deaf != num) || (mode_char == callerid_char && in_callerid != num)) {
            if (mode_char == deaf_char)
              in_deaf = num;
            else if (mode_char == callerid_char)
              in_callerid = num;
            dprintf(DP_SERVER, "MODE %s %c%c\n", botname, which, mode_char);
          }
        }
      }
    }
  } else if (var->flags & (VAR_STRING|VAR_WORD)) {
    char *olddata = ((char*) var->mem)[0] ? strdup((char*) var->mem) : NULL;

    if (data)
      strlcpy((char *) var->mem, data, var->size);
    else
      ((char *) var->mem)[0] = 0;

    if (!conf.bot->hub) {
      bool should_reset_monitor = 0;

      if (var->flags & VAR_JUPENICK) {
        //Don't allow setting to the same as origbotname
        if (data && !rfc_casecmp(jupenick, origbotname)) {
          ((char *) var->mem)[0] = 0;
          data = NULL;
        }

        //rolls = 0;
        //altnick_char = 0;
        // Don't send nick changes on restart, no need.
        if (server_online && !parsing_botset) {
          //New jupenick, no old value, or old value and it has changed.
          if (data && (!olddata || (olddata && strcmp(olddata, jupenick)))) {
            // If not on the new nick, jump to it
            if (!match_my_nick(jupenick)) {
              tried_jupenick = now;
              dprintf(DP_SERVER, "NICK %s\n", jupenick);
              should_reset_monitor = 1;
            }
          // Unset and there was an old value
          } else if (!data && olddata) { 
            // Unset jupenick, try for 'nick' now if we were on jupenick
            if (match_my_nick(olddata)) {
              altnick_char = rolls = 0;
              tried_nick = now;
              dprintf(DP_SERVER, "NICK %s\n", origbotname);
              should_reset_monitor = 1;
            }
          }
        }
      } else if (var->flags & VAR_NICK) {
        // Default to botnick
        if (!data)
          strlcpy((char *) var->mem, conf.bot->nick, var->size);

        // Only send nick changes if online and not loading
        if (server_online && !parsing_botset) {
          // the nick changed and not on the new nick already
          if ((olddata && strcmp(olddata, (char *) var->mem)) &&
              !match_my_nick((char *) var->mem) && 
              (!jupenick[0] || !match_my_nick(jupenick))) 
          {
            // Unset the rolls/altnick stuff as we're starting over from scratch.
            altnick_char = rolls = 0;
            dprintf(DP_SERVER, "NICK %s\n", (char *) var->mem);
            should_reset_monitor = 1;
          }
        }
      }
      if (server_online && should_reset_monitor)
        rehash_monitor_list();
    }
    if (olddata)
      free(olddata);
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
  } else if ((var->flags & VAR_SERVERS) && !strcmp(get_server_type(), var->name)) {
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

  if (!conf.bot->hub && !strcmp(var->name, "server-use-ssl")) {
    // Need to reload the server settings since we may want a different list now
    sdprintf("server-use-ssl changed, reprocessing server list");
    variable_t *servers = var_get_var_by_name(get_server_type());
    var_set_mem(servers, servers->ldata ? servers->ldata : servers->gdata ? servers->gdata : NULL);
  }

  // Check if should part/join channels based on groups changing
  if (!conf.bot->hub && !strcmp(var->name, "groups")) {
    if (server_online && !restarting && !loading && !reset_chans) {
      for (struct chanset_t* chan = chanset; chan; chan = chan->next) {
        check_shouldjoin(chan);
      }
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
        data = (char *) calloc(1, 11);
        simple_snprintf(data, 11, "%d", *(int *) (var->mem));
      }
    }
  } else if (var->flags & (VAR_BOOL)) {
    /* only bother setting if we have a value that's not 0 */
    if (*(int *) (var->mem)) {
      bool num = *(bool *) (var->mem);
      data = (char *) calloc(1, 2);
      /* Only actually set 0 or 1 */
      simple_snprintf(data, 2, "%d", num ? 1 : 0);
    }
  } else if (var->flags & (VAR_STRING|VAR_WORD)) {
    /* only bother setting if we have a non-empty string */
    if ((char *)var->mem && *(char *)var->mem)
      data = strdup((char *) var->mem);
  } else if (var->flags & VAR_RATE) {
    /* only bother setting if we don't have 0:0 */
    if ((*(rate_t *) (var->mem)).count && (*(rate_t *) (var->mem)).time) {
      data = (char *) calloc(1, 21);
      simple_snprintf(data, 21, "%d:%d", (*(rate_t *) (var->mem)).count, (*(rate_t *) (var->mem)).time);
    }
  } else if (var->flags & VAR_SERVERS) {
    /* only bother setting/checking if we have 'serverlist' alloc'd */
    if (*(struct server_list **)var->mem) {
      if (strcmp(var->name, get_server_type()))
        return NULL;

      struct server_list *n = NULL;
      char list[2048] = "", buf[101] = "";

      for (n = (*(struct server_list **)var->mem); n; n = n->next) {
        if (n->port && n->port != (ssl_use ? default_port_ssl : default_port))
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
  return strcasecmp(mi1->name, mi2->name);
}

static inline variable_t *var_get_var_by_name(const char *name)
{
  variable_t key;
  key.name = name;
  return (variable_t*) bsearch(&key, &vars, lengthof(vars) - 1, sizeof(variable_t), comp_variable_t);
}

const char *var_get_gdata(const char *name) {
  variable_t* var = var_get_var_by_name(name);
  return var && var->gdata ? var->gdata : NULL;
}

void var_set(variable_t *var, const char *target, const char *datain)
{
  if (target) {
    /*
     * Setting user entry despite possibly not setting memory due to
     * historically setting this after var_set() without checking
     * return value.
     */
    if (!parsing_botset) {
      var_set_userentry(target, var->name, datain);
    }

    /* Don't set locally if the variable doesn't permit it. */
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
    if (!strcasecmp(conf.bot->nick, target)) {
      domem = 1;				/* always set the mem if it's local */
      if (var->ldata)
        free(var->ldata);

#ifdef DEBUG
sdprintf("var: %s (local): %s", var->name, data ? data : "(NULL)");
#endif
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
#ifdef DEBUG
sdprintf("var: %s (global): %s", var->name, data ? data : "(NULL)");
#endif
    if (data && !clear)
      var->gdata = strdup(data);
    else
      var->gdata = var->def ? strdup(var->def) : NULL;

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

  if (!strcasecmp(conf.bot->nick, target))
    me = 1;

  if (me && conf.bot)
    u = conf.bot->u;
  else if (!me)
    u = get_user_by_handle(userlist, (char *) target);

  if (u) {
    struct xtra_key *xk = (struct xtra_key *) calloc(1, sizeof(struct xtra_key));

    xk->key = strdup(name);
    xk->data = (data && data[0]) ? strdup(data) : NULL;
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
  if (share && (conf.bot->hub || conf.bot->localhub))
    botnet_send_var_broad(idx, var);
  set_noshare = 0;
}

const char *var_get_bot_data(struct userrec *u, const char *name, bool useDefault)
{
  if (!u)
    return NULL;

  struct xtra_key *xk = NULL;

  xk = (struct xtra_key *) get_user(&USERENTRY_SET, u);
  while (xk && strcmp(xk->key, name))
    xk = xk->next;

  if (xk) {
    return xk->data;
  }
  if (useDefault) {
    variable_t *var = var_get_var_by_name(name);
    return var->gdata ? var->gdata : var->def;
  }
  return NULL;
}


void var_parse_my_botset()
{
  int i;
  struct xtra_key *xk = NULL, *x = NULL;

  /* look for local vars inside our own USERENTRY_SET and set them in our cfg struct */
  set_noshare = 1;                      /* why bother sharing out our LOCAL settings? */
  parsing_botset = 1;
  x = (struct xtra_key *) get_user(&USERENTRY_SET, conf.bot->u);
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
#ifdef DEBUG
      sdprintf("var[%s] nulled but we missed it!, reseting.", vars[i].name);
#endif
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
  const char *p = NULL;

  /* The first word only .. */
  if (!strcmp(var->name, "alias") && (p = strchr(element, ' ')))
    slen = p - element;
  /* Or the whole word */
  else
    slen = strlen(element);
  
  while ((item = strsep(&data, delim)))
    if (!strncasecmp(item, element, slen)) {
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
    botdata = (char *) var_get_bot_data(get_user_by_handle(userlist, (char *) botnick), var->name, true);
    olddata = botdata ? botdata : NULL;
  } else                                  //use global, no bot specified
    olddata = var->gdata ? var->gdata : NULL;

  /* Append to the olddata if there...*/
  size_t osiz = olddata ? strlen(olddata) : 0;

  if (olddata && osiz) {
    size_t esiz = strlen(element) + 1;		// element + ,

    data = (char *) calloc(1, osiz + esiz + 1);
    simple_snprintf(data, osiz + esiz + 1, "%s,%s", olddata, element);
  } else /* otherwise, just set the data to the element to be added */
    data = strdup(element);

  var_set(var, botnick, data);
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

  char *data = (char *) calloc(1, tsiz);
  char *word = NULL;
  while ((word = strsep(&olddata, delim))) {
    ++i;

    if ((num && num != i) || (!num && strncasecmp(word, element, elen))) {
      /* Reconstruct the left and right part of the list...*/
      if (data && data[0] && word && word[0]) {
        strlcat(data, ",", tsiz);
        strlcat(data, word, tsiz);
      } else
        strlcpy(data, word, tsiz);
    } else  /* minus the part we are removing ... */
      strlcpy(ret, word, sizeof(ret));
  }

  // If the data now matches the default global, just set to NULL
  if (botnick && var->gdata && data && !strcmp(var->gdata, data)) {
    data[0] = 0;
  }

  if (num <= i && ret[0]) {
    var_set(var, botnick, data);
  } else
    ret[0] = 0;
 
  free(data);
  free(olddatap);
  return ret;
}

void write_vars_and_cmdpass(bd::Stream& stream, int idx)
{
  putlog(LOG_DEBUG, "@", "Writing set entries...");
  stream << bd::String::printf(SET_NAME " - -\n");

  int i = 0;

  for (i = 0; vars[i].name; i++) {
    // If we're a localhub, dont share variables set to not leaf default unless we're linked to a hub.
    // Otherwise, we share out servers, and the child bots connect to the default list,
    // before ever receiving the actual list from the hub. (Along with stuff like realname)
    if (conf.bot->hub ||
        (conf.bot->localhub &&
         (!(vars[i].flags & VAR_NOLDEF) ||
          ((vars[i].flags & VAR_NOLDEF) && have_linked_to_hub))
         )) {
      /* send blanks if our variable isn't set, theirs MIGHT be set and needs to be UNSET */
      stream << bd::String::printf("@ %s %s\n", vars[i].name, vars[i].gdata ? vars[i].gdata : "");
    }
  }

  for (struct cmd_pass *cp = cmdpass; cp; cp = cp->next)
    stream << bd::String::printf("- %s %s\n", cp->name, cp->pass);
}

static void display_set_value(int idx, const variable_t *var, const char *botnick, bool format = false)
{
  char buf[51] = "";

  const char *data = NULL;
  if (botnick) {				//fetch data from bot's USERENTRY_SET
    struct userrec *botu = get_user_by_handle(userlist, (char *) botnick);
    const char *botdata = var_get_bot_data(botu, var->name);
    data = botdata ? botdata : NULL;
  } else {					//use global, no bot specified
    data = var->gdata ? var->gdata : NULL;
  }

  if (format) {
    simple_snprintf(buf, sizeof(buf), "(%-6s) %-16s: ", var_type_name(var->flags), var->name);
  } else {
    simple_snprintf(buf, sizeof(buf), "(%s) %s: ", var_type_name(var->flags), var->name);
  }
  dumplots(idx, buf, data ? (char *) data : (char *) "(not set)");
}

#define LIST_ADD  1
#define LIST_RM   2
#define LIST_SHOW 3
int cmd_set_real(const char *botnick, int idx, char *par)
{
  variable_t *var = NULL;
  char *name = NULL;
  const char *data = NULL, *botdata = NULL;
  int list = 0;
  bool notyes = 1, wildcard = 0;

  if (par[0] && !strncasecmp(par, "-yes", 4)) {
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
    else if (!strcasecmp(name, "list"))
      list = LIST_SHOW;
  }

  if (list) {
    if (list != LIST_SHOW && name[1] && strcasecmp(&name[1], "list"))
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
      dprintf(idx, "%s:\n", botnick);
    ishub = bot_hublevel(botu) == 999 ? 0 : 1;
  }

  if (!data) {
    // First determine which variables are going to be shown
    bd::Array<variable_t*> varsToShow;
    size_t i = 0;

    while (vars[i].name) {
      if (!name || wildcard) {	//not looping all, provided with one...
        if (vars[i].name) {
          var = &vars[i];
        }
      }

      if (wildcard && !wild_match(name, var->name)) {
        ++i;
        continue;
      }

      varsToShow << var;

      if (name && !wildcard) {
        break;
      }
      ++i;
    }

    // Then display them
    for (i = 0; i < varsToShow.length(); ++i) {
      var = varsToShow[i];
      botdata = NULL;
      
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
          const bool shouldFormat = varsToShow.length() > 1;
          display_set_value(idx, var, botnick, shouldFormat);
        }
      }
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

    if (!list && !strcmp(data, "-"))
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
          char *data_word = NULL;
          const char *p = NULL;
          if (!strcmp(var->name, "alias")) {
            if ((p = strchr(data, ' ')))
              data_word = strldup(data, p - data);
          } else
            data_word = (char*)data;
          dprintf(idx, "Item '%s' is already in the %s list.\n", data_word, var->name);
          if (p)
            free(data_word);
          return 0;
        } else if (var_add_list(botnick, var, data)) {
          dprintf(idx, "Added '%s' to %s list.\n", data, var->name);
          display_set_value(idx, var, botnick);
          return 1;
        }
      } else if (list == LIST_RM) {
        char *expanded_data = NULL;

        if ((expanded_data = var_rem_list(botnick, var, data)) && expanded_data[0]) {
          dprintf(idx, "Removed '%s' from %s list.\n", expanded_data, var->name);
          display_set_value(idx, var, botnick);
          return 1;
        } else if (!var_find_list(botnick, var, data)) {
          char *data_word = NULL;
          const char *p = NULL;
          if (!strcmp(var->name, "alias")) {
            if ((p = strchr(data, ' ')))
              data_word = strldup(data, p - data);
          } else
            data_word = (char*)data;
          dprintf(idx, "Item '%s' does not exist in the %s list.\n", data, var->name);
          if (p)
            free(data_word);
          return 0;
        }
      }
      dprintf(idx, "Failed to modify %s list.\n", var->name);
      return 0;
    } else {
      /* If no data or data is not -
         sanitize the data */
      char *sdata = NULL;
      /* Make a temporary to free at the end */
      if (data && strcmp(data, "-") && (!var->def || (var->def && strcmp(data, var->def)))) {
        sdata = var_sanitize(var, data);
        data = sdata;
      }

      var_set(var, botnick, data);

      display_set_value(idx, var, botnick);

      if (sdata)
        free(sdata);
    }
    return 1;
  }
  return 0;
}
/* vim: set sts=2 sw=2 ts=8 et: */
