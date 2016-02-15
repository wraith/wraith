/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
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

/*
 * cmdsserv.c -- part of server.mod
 *   handles commands from a user via dcc that cause server interaction
 *
 */


static void cmd_servers(int idx, char *par)
{
  struct server_list *x = serverlist;
  int i;
  char s[1024] = "";

  putlog(LOG_CMDS, "*", "#%s# servers", dcc[idx].nick);
  if (!x) {
    dprintf(idx, "There are no servers in the server list.\n");
  } else {
    dprintf(idx, "Server list:\n");
    i = 0;
    for (; x; x = x->next) {
        simple_snprintf(s, sizeof s, "  %s:%d %s", x->name, 
		     x->port ? x->port : (ssl_use ? default_port_ssl : default_port),
		     (i == curserv) ? "<- I am here" : "");
      dprintf(idx, "%s\n", s);
      i++;
    }
    dprintf(idx, "End of server list.\n"); 
  }
}

static void cmd_dump(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# dump %s", dcc[idx].nick, par);

  if (!isowner(dcc[idx].nick)) {
    putlog(LOG_WARN, "*", "%s attempted 'dump' %s", dcc[idx].nick, par);
    dprintf(idx, "dump is only available to permanent owners.\n");
    return;
  }
  if (!par[0]) {
    dprintf(idx, "Usage: dump <server stuff>\n");
    return;
  }

  dprintf(DP_DUMP, "%s\n", replace_vars(par));
}

static void cmd_umode(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# umode %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: umode <+flags>\n");
    return;
  }

  dprintf(DP_SERVER, "MODE %s %s\n", botname, par);
}

static void cmd_jump(int idx, char *par)
{
  char *other = NULL, *p = NULL;
  int port;

  if (par[0]) {
    other = newsplit(&par);
    port = atoi(newsplit(&par));

    if ((p = strchr(other, ':'))) {
      *p = 0;
      p++;
      if (!port)
        port = atoi(p);
    }
    if (!port)
      port = (ssl_use ? default_port_ssl : default_port);
    putlog(LOG_CMDS, "*", "#%s# jump %s %d %s", dcc[idx].nick, other, port, par);
    strlcpy(newserver, other, sizeof newserver);
    newserverport = port;
    strlcpy(newserverpass, par, sizeof newserverpass);
  } else
    putlog(LOG_CMDS, "*", "#%s# jump", dcc[idx].nick);
  dprintf(idx, "Jumping servers...\n");
  nuke_server("changing servers");
  cycle_time = 0;
}

static void cmd_keyx(int idx, char *par) {
  putlog(LOG_CMDS, "*", "#%s# keyx %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: keyx <nick>\n");
    return;
  }

  if (strchr(CHANMETA, par[0])) {
    dprintf(idx, "Error: Cannot key-exchange with a channel.\n");
    return;
  }

  if (!server_online) {
    dprintf(idx, "Error: Not online.\n");
    return;
  }

  char *nick = newsplit(&par);
  keyx(nick, "Requested");
  return;
}

static void cmd_setkey(int idx, char *par) {
  putlog(LOG_CMDS, "*", "#%s# setkey %s", dcc[idx].nick, par);

  const bool target_is_chan = par[0] && strchr(CHANMETA, par[0]);

  if (!par[0] || target_is_chan) {
    dprintf(idx, "Usage: setkey <nick> [key|rand]\n");
    if (target_is_chan) {
      dprintf(idx, "Use 'chanset fish-key' to set a key for a channel.\n");
    }
    return;
  }

  char *target = newsplit(&par);
  char *newkey = newsplit(&par);
  bool have_key = FishKeys.contains(target);

  if (!newkey[0]) {
    // Clear the key
    if (have_key) {
      set_fish_key(target, "");
      dprintf(idx, "Key cleared for '%s'\n", target);
    } else {
      dprintf(idx, "No key found for '%s'\n", target);
    }
  } else {
    // Set a new key
    set_fish_key(target, newkey);
    fish_data_t* fishData = FishKeys[target];
    dprintf(idx, "Set key for '%s' to: %s", target, fishData->sharedKey.c_str());
  }
  return;
}

static void cmd_clearqueue(int idx, char *par)
{
  int msgs;

  putlog(LOG_CMDS, "*", "#%s# clearqueue %s", dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, "Usage: clearqueue <mode|server|help|all>\n");
    return;
  }
  if (!strcasecmp(par, "all")) {
    msgs = modeq.tot + mq.tot + hq.tot + aq.tot;
    msgq_clear(&modeq);
    msgq_clear(&mq);
    msgq_clear(&hq);
    msgq_clear(&aq);
    burst = 0;
    double_warned = 0;
    dprintf(idx, "Removed %d message%s from all queues.\n", msgs, 
        (msgs != 1) ? "s" : "");
  } else if (!strcasecmp(par, "mode")) {
    msgs = modeq.tot;
    msgq_clear(&modeq);
    if (mq.tot == 0)
      burst = 0;
    double_warned = 0;
    dprintf(idx, "Removed %d message%s from the mode queue.\n", msgs, 
        (msgs != 1) ? "s" : "");
  } else if (!strcasecmp(par, "help")) {
    msgs = hq.tot;
    msgq_clear(&hq);
    double_warned = 0;
    dprintf(idx, "Removed %d message%s from the help queue.\n", msgs,
        (msgs != 1) ? "s" : "");
  } else if (!strcasecmp(par, "play")) {
    msgs = aq.tot;
    msgq_clear(&aq);
    double_warned = 0;
    dprintf(idx, "Removed %d message%s from the play queue.\n", msgs,
        (msgs != 1) ? "s" : "");
  } else if (!strcasecmp(par, "server")) {
    msgs = mq.tot;
    msgq_clear(&mq);
    if (modeq.tot == 0)
      burst = 0;
    double_warned = 0;
    dprintf(idx, "Removed %d message%s from the server queue.\n", msgs,
        (msgs != 1) ? "s" : "");
  } else {
    dprintf(idx, "Usage: clearqueue <mode|server|help|all>\n");
    return;
  }
}
/* Function call should be:
 *   int cmd_whatever(idx,"parameters");
 *
 * As with msg commands, function is responsible for any logging.
 */
static cmd_t C_dcc_serv[] =
{
  {"clearqueue",	"m",	(Function) cmd_clearqueue,	NULL, LEAF|AUTH},
  {"dump",		"a",	(Function) cmd_dump,		NULL, LEAF},
  {"jump",		"m",	(Function) cmd_jump,		NULL, LEAF},
  {"keyx",		"o",	(Function) cmd_keyx,		NULL, LEAF},
  {"servers",		"m",	(Function) cmd_servers,		NULL, LEAF},
  {"setkey",		"m",	(Function) cmd_setkey,		NULL, LEAF},
  {"umode",		"m",	(Function) cmd_umode,		NULL, LEAF},
  {NULL,		NULL,	NULL,				NULL, 0}
};

/* vim: set sts=2 sw=2 ts=8 et: */
