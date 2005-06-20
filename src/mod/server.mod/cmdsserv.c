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
		     x->port ? x->port : default_port, 
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
    dprintf(idx, "What?  You need '%shelp'\n", settings.dcc_prefix);
    return;
  }
  if (!par[0]) {
    dprintf(idx, "Usage: dump <server stuff>\n");
    return;
  }

  dprintf(DP_DUMP, "%s\n", par);
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
      port = default_port;
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

static void cmd_clearqueue(int idx, char *par)
{
  int msgs;

  putlog(LOG_CMDS, "*", "#%s# clearqueue %s", dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, "Usage: clearqueue <mode|server|help|all>\n");
    return;
  }
  if (!egg_strcasecmp(par, "all")) {
    msgs = modeq.tot + mq.tot + hq.tot;
    msgq_clear(&modeq);
    msgq_clear(&mq);
    msgq_clear(&hq);
    burst = 0;
    double_warned = 0;
    dprintf(idx, "Removed %d message%s from all queues.\n", msgs, 
        (msgs != 1) ? "s" : "");
  } else if (!egg_strcasecmp(par, "mode")) {
    msgs = modeq.tot;
    msgq_clear(&modeq);
    if (mq.tot == 0)
      burst = 0;
    double_warned = 0;
    dprintf(idx, "Removed %d message%s from the mode queue.\n", msgs, 
        (msgs != 1) ? "s" : "");
  } else if (!egg_strcasecmp(par, "help")) {
    msgs = hq.tot;
    msgq_clear(&hq);
    double_warned = 0;
    dprintf(idx, "Removed %d message%s from the help queue.\n", msgs,
        (msgs != 1) ? "s" : "");
  } else if (!egg_strcasecmp(par, "server")) {
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
  {"clearqueue",	"m",	(Function) cmd_clearqueue,	NULL, LEAF},
  {"dump",		"a",	(Function) cmd_dump,		NULL, LEAF},
  {"jump",		"m",	(Function) cmd_jump,		NULL, LEAF},
  {"servers",		"m",	(Function) cmd_servers,		NULL, LEAF},
  {NULL,		NULL,	NULL,				NULL, 0}
};

