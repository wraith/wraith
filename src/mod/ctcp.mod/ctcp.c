#ifdef LEAF
#define MODULE_NAME "ctcp"
#define MAKING_CTCP
#include "ctcp.h"
#include "src/mod/module.h"
#include "server.mod/server.h"
#include <netinet/in.h>
#include <arpa/inet.h>
static Function *global = NULL, *server_funcs = NULL;
static char ctcp_version[121];
static char ctcp_finger[121];
static char ctcp_userinfo[121];
static int ctcp_mode = 0;
static int listen_time = 0;
static void
ctcp_minutely ()
{
  int i;
  if (listen_time <= 0)
    {
      for (i = 0; i < dcc_total; i++)
	{
	  if ((dcc[i].type->flags & DCT_LISTEN)
	      && (!strcmp (dcc[i].nick, "(telnet)")))
	    {
	      putlog (LOG_DEBUG, "*", "Closing listening port %d",
		      dcc[i].port);
	      killsock (dcc[i].sock);
	      lostdcc (i);
	      break;
	    }
	}
    }
  else
    listen_time--;
}
static int
ctcp_FINGER (char *nick, char *uhost, char *handle, char *object,
	     char *keyword, char *text)
{
  if (ctcp_mode != 1 && ctcp_finger[0])
    simple_sprintf (ctcp_reply, "%s\001FINGER %s\001", ctcp_reply,
		    ctcp_finger);
  return 1;
}
static int
ctcp_ECHOERR (char *nick, char *uhost, char *handle, char *object,
	      char *keyword, char *text)
{
  if (ctcp_mode != 1 && strlen (text) <= 80)
    simple_sprintf (ctcp_reply, "%s\001%s %s\001", ctcp_reply, keyword, text);
  return 1;
}
static int
ctcp_PING (char *nick, char *uhost, char *handle, char *object, char *keyword,
	   char *text)
{
  struct userrec *u = get_user_by_handle (userlist, handle);
  int atr = u ? u->flags : 0;
  if ((ctcp_mode != 1 || (atr & USER_OP)) && strlen (text) <= 80)
    simple_sprintf (ctcp_reply, "%s\001%s %s\001", ctcp_reply, keyword, text);
  return 1;
}
static int
ctcp_VERSION (char *nick, char *uhost, char *handle, char *object,
	      char *keyword, char *text)
{
  if (ctcp_mode != 1 && ctcp_version[0])
    simple_sprintf (ctcp_reply, "%s\001VERSION %s\001", ctcp_reply,
		    ctcp_version);
  return 1;
}
static int
ctcp_USERINFO (char *nick, char *uhost, char *handle, char *object,
	       char *keyword, char *text)
{
  if (ctcp_mode != 1 && ctcp_userinfo[0])
    simple_sprintf (ctcp_reply, "%s\001USERINFO %s\001", ctcp_reply,
		    ctcp_userinfo);
  return 1;
}
static int
ctcp_CLIENTINFO (char *nick, char *uhosr, char *handle, char *object,
		 char *keyword, char *msg)
{
  char *p = NULL;
  if (ctcp_mode == 1)
    return 1;
  else if (!msg[0])
    p = CLIENTINFO;
  else if (!egg_strcasecmp (msg, "sed"))
    p = CLIENTINFO_SED;
  else if (!egg_strcasecmp (msg, "version"))
    p = CLIENTINFO_VERSION;
  else if (!egg_strcasecmp (msg, "clientinfo"))
    p = CLIENTINFO_CLIENTINFO;
  else if (!egg_strcasecmp (msg, "userinfo"))
    p = CLIENTINFO_USERINFO;
  else if (!egg_strcasecmp (msg, "errmsg"))
    p = CLIENTINFO_ERRMSG;
  else if (!egg_strcasecmp (msg, "finger"))
    p = CLIENTINFO_FINGER;
  else if (!egg_strcasecmp (msg, "time"))
    p = CLIENTINFO_TIME;
  else if (!egg_strcasecmp (msg, "action"))
    p = CLIENTINFO_ACTION;
  else if (!egg_strcasecmp (msg, "dcc"))
    p = CLIENTINFO_DCC;
  else if (!egg_strcasecmp (msg, "utc"))
    p = CLIENTINFO_UTC;
  else if (!egg_strcasecmp (msg, "ping"))
    p = CLIENTINFO_PING;
  else if (!egg_strcasecmp (msg, "echo"))
    p = CLIENTINFO_ECHO;
  if (p == NULL)
    {
      simple_sprintf (ctcp_reply,
		      "%s\001ERRMSG CLIENTINFO: %s is not a valid function\001",
		      ctcp_reply, msg);
    }
  else
    simple_sprintf (ctcp_reply, "%s\001CLIENTINFO %s\001", ctcp_reply, p);
  return 1;
}
static int
ctcp_TIME (char *nick, char *uhost, char *handle, char *object, char *keyword,
	   char *text)
{
  char tms[25];
  if (ctcp_mode == 1)
    return 1;
  strncpy (tms, ctime (&now), 24);
  tms[24] = 0;
  simple_sprintf (ctcp_reply, "%s\001TIME %s\001", ctcp_reply, tms);
  return 1;
}
static int
ctcp_CHAT (char *nick, char *uhost, char *handle, char *object, char *keyword,
	   char *text)
{
  struct userrec *u = get_user_by_handle (userlist, handle);
  int i, ix = (-1);
  if (!ischanhub () && !issechub ())
    return 0;
  if (u_pass_match (u, "-"))
    {
      simple_sprintf (ctcp_reply, "%s\001ERROR no password set\001",
		      ctcp_reply);
      return 1;
    }
  for (i = 0; i < dcc_total; i++)
    {
      if ((dcc[i].type->flags & DCT_LISTEN)
	  && (!strcmp (dcc[i].nick, "(telnet)")))
	ix = i;
    }
  if (dcc_total == max_dcc || (ix < 0 && (ix = listen_all (0, 0)) < 0))
    simple_sprintf (ctcp_reply, "%s\001ERROR no telnet port\001", ctcp_reply);
  else
    {
      if (listen_time <= 2)
	listen_time++;
      dprintf (DP_HELP, "PRIVMSG %s :\001DCC CHAT chat %lu %u\001\n", nick,
	       iptolong (getmyip ()), dcc[ix].port);
    }
  return 1;
}
static cmd_t myctcp[] =
  { {"FINGER", "", ctcp_FINGER, NULL}, {"ECHO", "", ctcp_ECHOERR, NULL},
  {"PING", "", ctcp_PING, NULL}, {"ERRMSG", "", ctcp_ECHOERR, NULL},
  {"VERSION", "", ctcp_VERSION, NULL}, {"USERINFO", "", ctcp_USERINFO, NULL},
  {"CLIENTINFO", "", ctcp_CLIENTINFO, NULL}, {"TIME", "", ctcp_TIME, NULL},
  {"CHAT", "", ctcp_CHAT, NULL}, {NULL, NULL, NULL, NULL} };
static tcl_strings mystrings[] =
  { {"ctcp-version", ctcp_version, 120, 0}, {"ctcp-finger", ctcp_finger, 120,
					     0}, {"ctcp-userinfo",
						  ctcp_userinfo, 120, 0},
  {NULL, NULL, 0, 0} };
static tcl_ints myints[] = { {"ctcp-mode", &ctcp_mode}, {NULL, NULL} };
static char *
ctcp_close ()
{
  rem_tcl_strings (mystrings);
  rem_tcl_ints (myints);
  rem_builtins (H_ctcp, myctcp);
  module_undepend (MODULE_NAME);
  return NULL;
}
EXPORT_SCOPE char *ctcp_start ();
static Function ctcp_table[] =
  { (Function) ctcp_start, (Function) ctcp_close, (Function) NULL,
(Function) NULL, };
char *
ctcp_start (Function * global_funcs)
{
  global = global_funcs;
  module_register (MODULE_NAME, ctcp_table, 1, 0);
  if (!(server_funcs = module_depend (MODULE_NAME, "server", 1, 0)))
    {
      module_undepend (MODULE_NAME);
      return "This module requires server module 1.0 or later.";
    }
  add_tcl_strings (mystrings);
  add_tcl_ints (myints);
  add_builtins (H_ctcp, myctcp);
  add_hook (HOOK_MINUTELY, (Function) ctcp_minutely);
  if (!ctcp_version[0])
    {
      strncpy (ctcp_version, ver, 120);
      ctcp_version[120] = 0;
    }
  if (!ctcp_finger[0])
    {
      strncpy (ctcp_finger, ver, 120);
      ctcp_finger[120] = 0;
    }
  if (!ctcp_userinfo[0])
    {
      strncpy (ctcp_userinfo, ver, 120);
      ctcp_userinfo[120] = 0;
    }
  return NULL;
}
#endif
