#include "main.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "chan.h"
#include "tandem.h"
#include "modules.h"
#include <pwd.h>
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif
#include "stat.h"
#include "bg.h"
extern struct userrec *userlist;
extern struct dcc_t *dcc;
extern struct chanset_t *chanset;
extern char version[], origbotname[], botname[], admin[], network[],
  motdfile[], ver[], botnetnick[], bannerfile[], logfile_suffix[], textdir[],
  userfile[], *binname, pid_file[], netpass[], tempdir[], mhub[];
extern int backgrd, con_chan, term_z, use_stderr, dcc_total, timesync,
#ifdef HUB
  my_port,
#endif
  keep_all_logs, quick_logs, strict_host, loading, localhub;
extern time_t now;
extern Tcl_Interp *interp;
void detected (int, char *);
int shtime = 1;
log_t *logs = 0;
int max_logs = 5;
int max_logsize = 0;
int conmask = LOG_MODES | LOG_CMDS | LOG_MISC;
int debug_output = 1;
struct cfg_entry CFG_MOTD =
  { "motd", CFGF_GLOBAL, NULL, NULL, NULL, NULL, NULL };
void
fork_lchanged (struct cfg_entry *cfgent, char *oldval, int *valid)
{
  if (!cfgent->ldata)
    return;
  if (atoi (cfgent->ldata) <= 0)
    *valid = 0;
}

void
fork_gchanged (struct cfg_entry *cfgent, char *oldval, int *valid)
{
  if (!cfgent->gdata)
    return;
  if (atoi (cfgent->gdata) <= 0)
    *valid = 0;
}

void
fork_describe (struct cfg_entry *cfgent, int idx)
{
  dprintf (idx,
	   STR
	   ("fork-interval is number of seconds in between each fork() call made by the bot, to change process ID and reset cpu usage counters.\n"));
} struct cfg_entry CFG_FORKINTERVAL =
  { "fork-interval", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL, fork_gchanged,
fork_lchanged, fork_describe };
void detect_describe (struct cfg_entry *cfgent, int idx);
void detect_lchanged (struct cfg_entry *cfgent, char *oldval, int *valid);
void detect_gchanged (struct cfg_entry *cfgent, char *oldval, int *valid);
#ifdef S_LASTCHECK
struct cfg_entry CFG_LOGIN =
  { "login", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL, detect_gchanged,
detect_lchanged, detect_describe };
#endif
#ifdef S_ANTITRACE
struct cfg_entry CFG_TRACE =
  { "trace", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL, detect_gchanged,
detect_lchanged, detect_describe };
#endif
#ifdef S_PROMISC
struct cfg_entry CFG_PROMISC =
  { "promisc", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL, detect_gchanged,
detect_lchanged, detect_describe };
#endif
#ifdef S_PROCESSCHECK
struct cfg_entry CFG_BADPROCESS =
  { "bad-process", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL, detect_gchanged,
detect_lchanged, detect_describe };
struct cfg_entry CFG_PROCESSLIST =
  { "process-list", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL, NULL, NULL,
detect_describe };
#endif
void
detect_describe (struct cfg_entry *cfgent, int idx)
{
#ifdef S_LASTCHECK
  if (cfgent == &CFG_LOGIN)
    dprintf (idx,
	     STR
	     ("login sets how to handle someone logging in to the shell\n"));
  else
#endif
#ifdef S_ANTITRACE
  if (cfgent == &CFG_TRACE)
    dprintf (idx,
	     STR
	     ("trace sets how to handle someone tracing/debugging the bot\n"));
  else
#endif
#ifdef S_PROMISC
  if (cfgent == &CFG_PROMISC)
    dprintf (idx,
	     STR
	     ("promisc sets how to handle when a interface is set to promiscous mode\n"));
  else
#endif
#ifdef S_PROCESSCHECK
  if (cfgent == &CFG_BADPROCESS)
    dprintf (idx,
	     STR
	     ("bad-process sets how to handle when a running process not listed in process-list is detected\n"));
  else if (cfgent == &CFG_PROCESSLIST)
    dprintf (idx,
	     STR
	     ("process-list is a comma-separated list of \"expected processes\" running on the bots uid\n"));
  else
#endif
    {
      putlog (LOG_ERRORS, "*",
	      STR
	      ("huh? detect_describe called with unknown config entry\n"));
      return;
    }
  dprintf (idx,
	   STR
	   ("Valid settings are: nocheck, ignore, warn, die, reject, suicide\n"));
}

void
detect_lchanged (struct cfg_entry *cfgent, char *oldval, int *valid)
{
  char *p = cfgent->ldata;
  if (!p)
    *valid = 1;
  else if (strcmp (p, STR ("ignore")) && strcmp (p, STR ("die"))
	   && strcmp (p, STR ("reject")) && strcmp (p, STR ("suicide"))
	   && strcmp (p, STR ("nocheck")) && strcmp (p, STR ("warn")))
    *valid = 0;
}

void
detect_gchanged (struct cfg_entry *cfgent, char *oldval, int *valid)
{
  char *p = (char *) cfgent->ldata;
  if (!p)
    *valid = 1;
  else if (strcmp (p, STR ("ignore")) && strcmp (p, STR ("die"))
	   && strcmp (p, STR ("reject")) && strcmp (p, STR ("suicide"))
	   && strcmp (p, STR ("nocheck")) && strcmp (p, STR ("warn")))
    *valid = 0;
}

#ifdef S_DCCPASS
struct cmd_pass *cmdpass = NULL;
#endif
#define upcase(c) (((c)>='a' && (c)<='z') ? (c)-'a'+'A' : (c))
#if !HAVE_STRCASECMP
#define strcasecmp strcasecmp2
#endif
int
strcasecmp2 (char *s1, char *s2)
{
  while ((*s1) && (*s2) && (upcase (*s1) == upcase (*s2)))
    {
      s1++;
      s2++;
    }
  return upcase (*s1) - upcase (*s2);
}

#ifdef HUB
void
servers_describe (struct cfg_entry *entry, int idx)
{
  dprintf (idx,
	   STR
	   ("servers is a comma-separated list of servers the bot will use\n"));
} void
servers_changed (struct cfg_entry *entry, char *olddata, int *valid)
{
} void
servers6_describe (struct cfg_entry *entry, int idx)
{
  dprintf (idx,
	   STR
	   ("servers6 is a comma-separated list of servers the bot will use (FOR IPv6)\n"));
} void
servers6_changed (struct cfg_entry *entry, char *olddata, int *valid)
{
} void
nick_describe (struct cfg_entry *entry, int idx)
{
  dprintf (idx,
	   "nick is the bots preferred nick when connecting/using .resetnick\n");
} void
nick_changed (struct cfg_entry *entry, char *olddata, int *valid)
{
} void
realname_describe (struct cfg_entry *entry, int idx)
{
  dprintf (idx, STR ("realname is the bots \"real name\" when connecting\n"));
} void
realname_changed (struct cfg_entry *entry, char *olddata, int *valid)
{
} struct cfg_entry CFG_SERVERS =
  { "servers", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL, servers_changed,
servers_changed, servers_describe };
struct cfg_entry CFG_SERVERS6 =
  { "servers6", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL, servers6_changed,
servers6_changed, servers6_describe };
struct cfg_entry CFG_NICK =
  { "nick", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL, nick_changed, nick_changed,
nick_describe };
struct cfg_entry CFG_REALNAME =
  { "realname", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL, realname_changed,
realname_changed, realname_describe };
void
getin_describe (struct cfg_entry *cfgent, int idx)
{
  if (!strcmp (cfgent->name, STR ("op-bots")))
    dprintf (idx,
	     STR
	     ("op-bots is number of bots to ask every time a oprequest is to be made\n"));
  else if (!strcmp (cfgent->name, STR ("in-bots")))
    dprintf (idx,
	     STR
	     ("in-bots is number of bots to ask every time a inrequest is to be made\n"));
  else if (!strcmp (cfgent->name, STR ("op-requests")))
    dprintf (idx,
	     STR
	     ("op-requests (requests:seconds) limits how often the bot will ask for ops\n"));
  else if (!strcmp (cfgent->name, STR ("lag-threshold")))
    dprintf (idx,
	     STR
	     ("lag-threshold is maximum acceptable server lag for the bot to send/honor requests\n"));
  else if (!strcmp (cfgent->name, STR ("op-time-slack")))
    dprintf (idx,
	     STR
	     ("op-time-slack is number of seconds a opcookies encoded time value can be off from the bots current time\n"));
  else if (!strcmp (cfgent->name, STR ("lock-threshold")))
    dprintf (idx,
	     STR
	     ("Format H:L. When at least H hubs but L or less leafs are linked, lock all channels\n"));
  else if (!strcmp (cfgent->name, STR ("kill-threshold")))
    dprintf (idx,
	     STR
	     ("When more than kill-threshold bots have been killed/k-lined the last minute, channels are locked\n"));
  else if (!strcmp (cfgent->name, STR ("fight-threshold")))
    dprintf (idx,
	     STR
	     ("When more than fight-threshold ops/deops/kicks/bans/unbans altogether have happened on a channel in one minute, the channel is locked\n"));
  else
    {
      dprintf (idx, STR ("No description for %s ???\n"), cfgent->name);
      putlog (LOG_ERRORS, "*",
	      STR ("getin_describe() called with unknown config entry %s"),
	      cfgent->name);
    }
}
void
getin_changed (struct cfg_entry *cfgent, char *oldval, int *valid)
{
  int i;
  if (!cfgent->gdata)
    return;
  *valid = 0;
  if (!strcmp (cfgent->name, STR ("op-requests")))
    {
      int L, R;
      char *value = cfgent->gdata;
      L = atoi (value);
      value = strchr (value, ':');
      if (!value)
	return;
      value++;
      R = atoi (value);
      if ((R >= 60) || (R <= 3) || (L < 1) || (L > R))
	return;
      *valid = 1;
      return;
    }
  if (!strcmp (cfgent->name, STR ("lock-threshold")))
    {
      int L, R;
      char *value = cfgent->gdata;
      L = atoi (value);
      value = strchr (value, ':');
      if (!value)
	return;
      value++;
      R = atoi (value);
      if ((R >= 1000) || (R < 0) || (L < 0) || (L > 100))
	return;
      *valid = 1;
      return;
    }
  i = atoi (cfgent->gdata);
  if (!strcmp (cfgent->name, STR ("op-bots")))
    {
      if ((i < 1) || (i > 10))
	return;
    }
  else if (!strcmp (cfgent->name, STR ("invite-bots")))
    {
      if ((i < 1) || (i > 10))
	return;
    }
  else if (!strcmp (cfgent->name, STR ("key-bots")))
    {
      if ((i < 1) || (i > 10))
	return;
    }
  else if (!strcmp (cfgent->name, STR ("limit-bots")))
    {
      if ((i < 1) || (i > 10))
	return;
    }
  else if (!strcmp (cfgent->name, STR ("unban-bots")))
    {
      if ((i < 1) || (i > 10))
	return;
    }
  else if (!strcmp (cfgent->name, STR ("lag-threshold")))
    {
      if ((i < 3) || (i > 60))
	return;
    }
  else if (!strcmp (cfgent->name, STR ("fight-threshold")))
    {
      if (i && ((i < 50) || (i > 1000)))
	return;
    }
  else if (!strcmp (cfgent->name, STR ("kill-threshold")))
    {
      if ((i < 0) || (i >= 200))
	return;
    }
  else if (!strcmp (cfgent->name, STR ("op-time-slack")))
    {
      if ((i < 30) || (i > 1200))
	return;
    }
  *valid = 1;
  return;
}
struct cfg_entry CFG_OPBOTS =
  { "op-bots", CFGF_GLOBAL, NULL, NULL, getin_changed, NULL, getin_describe };
struct cfg_entry CFG_INBOTS =
  { "in-bots", CFGF_GLOBAL, NULL, NULL, getin_changed, NULL, getin_describe };
struct cfg_entry CFG_LAGTHRESHOLD =
  { "lag-threshold", CFGF_GLOBAL, NULL, NULL, getin_changed, NULL,
getin_describe };
struct cfg_entry CFG_OPREQUESTS =
  { "op-requests", CFGF_GLOBAL, NULL, NULL, getin_changed, NULL,
getin_describe };
struct cfg_entry CFG_OPTIMESLACK =
  { "op-time-slack", CFGF_GLOBAL, NULL, NULL, getin_changed, NULL,
getin_describe };
#ifdef G_AUTOLOCK
struct cfg_entry CFG_LOCKTHRESHOLD =
  { "lock-threshold", CFGF_GLOBAL, NULL, NULL, getin_changed, NULL,
getin_describe };
struct cfg_entry CFG_KILLTHRESHOLD =
  { "kill-threshold", CFGF_GLOBAL, NULL, NULL, getin_changed, NULL,
getin_describe };
struct cfg_entry CFG_FIGHTTHRESHOLD =
  { "fight-threshold", CFGF_GLOBAL, NULL, NULL, getin_changed, NULL,
getin_describe };
#endif
#endif
int cfg_count = 0;
struct cfg_entry **cfg = NULL;
int cfg_noshare = 0;
int
expmem_misc ()
{
#ifdef S_DCCPASS
  struct cmd_pass *cp = NULL;
#endif
  int tot = 0, i;
  for (i = 0; i < cfg_count; i++)
    {
      tot += sizeof (void *);
      if (cfg[i]->gdata)
	tot += strlen (cfg[i]->gdata) + 1;
      if (cfg[i]->ldata)
	tot += strlen (cfg[i]->ldata) + 1;
    }
#ifdef S_DCCPASS
  for (cp = cmdpass; cp; cp = cp->next)
    {
      tot += sizeof (struct cmd_pass) + strlen (cp->name) + 1;
    }
#endif
  return tot + (max_logs * sizeof (log_t));
}

void
init_misc ()
{
  static int last = 0;
  if (max_logs < 1)
    max_logs = 1;
  if (logs)
    logs = nrealloc (logs, max_logs * sizeof (log_t));
  else
    logs = nmalloc (max_logs * sizeof (log_t));
  for (; last < max_logs; last++)
    {
      logs[last].filename = logs[last].chname = NULL;
      logs[last].mask = 0;
      logs[last].f = NULL;
      logs[last].szlast[0] = 0;
      logs[last].repeats = 0;
      logs[last].flags = 0;
    }
  add_cfg (&CFG_MOTD);
  add_cfg (&CFG_FORKINTERVAL);
#ifdef S_LASTCHECK
  add_cfg (&CFG_LOGIN);
#endif
#ifdef S_ANTITRACE
  add_cfg (&CFG_TRACE);
#endif
#ifdef S_PROMISC
  add_cfg (&CFG_PROMISC);
#endif
#ifdef S_PROCESSCHECK
  add_cfg (&CFG_BADPROCESS);
  add_cfg (&CFG_PROCESSLIST);
#endif
#ifdef HUB
  add_cfg (&CFG_NICK);
  add_cfg (&CFG_SERVERS);
  add_cfg (&CFG_SERVERS6);
  add_cfg (&CFG_REALNAME);
  set_cfg_str (NULL, STR ("realname"), "A deranged product of evil coders");
  add_cfg (&CFG_OPBOTS);
  add_cfg (&CFG_INBOTS);
  add_cfg (&CFG_LAGTHRESHOLD);
  add_cfg (&CFG_OPREQUESTS);
  add_cfg (&CFG_OPTIMESLACK);
#ifdef G_AUTOLOCK
  add_cfg (&CFG_LOCKTHRESHOLD);
  add_cfg (&CFG_KILLTHRESHOLD);
  add_cfg (&CFG_FIGHTTHRESHOLD);
#endif
#endif
}

int
is_file (const char *s)
{
  struct stat ss;
  int i = stat (s, &ss);
  if (i < 0)
    return 0;
  if ((ss.st_mode & S_IFREG) || (ss.st_mode & S_IFLNK))
    return 1;
  return 0;
}

int
egg_strcatn (char *dst, const char *src, size_t max)
{
  size_t tmpmax = 0;
  while (*dst && max > 0)
    {
      dst++;
      max--;
    }
  tmpmax = max;
  while (*src && max > 1)
    {
      *dst++ = *src++;
      max--;
    }
  *dst = 0;
  return tmpmax - max;
}

int
my_strcpy (register char *a, register char *b)
{
  register char *c = b;
  while (*b)
    *a++ = *b++;
  *a = *b;
  return b - c;
}

void
splitc (char *first, char *rest, char divider)
{
  char *p = strchr (rest, divider);
  if (p == NULL)
    {
      if (first != rest && first)
	first[0] = 0;
      return;
    }
  *p = 0;
  if (first != NULL)
    strcpy (first, rest);
  if (first != rest)
    strcpy (rest, p + 1);
}

void
splitcn (char *first, char *rest, char divider, size_t max)
{
  char *p = strchr (rest, divider);
  if (p == NULL)
    {
      if (first != rest && first)
	first[0] = 0;
      return;
    }
  *p = 0;
  if (first != NULL)
    strncpyz (first, rest, max);
  if (first != rest)
    strcpy (rest, p + 1);
}

char *
splitnick (char **blah)
{
  char *p = strchr (*blah, '!'), *q = *blah;
  if (p)
    {
      *p = 0;
      *blah = p + 1;
      return q;
    }
  return "";
}

char *
newsplit (char **rest)
{
  register char *o, *r;
  if (!rest)
    return *rest = "";
  o = *rest;
  while (*o == ' ')
    o++;
  r = o;
  while (*o && (*o != ' '))
    o++;
  if (*o)
    *o++ = 0;
  *rest = o;
  return r;
}

void
maskhost (const char *s, char *nw)
{
  register const char *p, *q, *e, *f;
  int i;
  *nw++ = '*';
  *nw++ = '!';
  p = (q = strchr (s, '!')) ? q + 1 : s;
  if ((q = strchr (p, '@')))
    {
      int fl = 0;
      if ((q - p) > 9)
	{
	  nw[0] = '*';
	  p = q - 7;
	  i = 1;
	}
      else
	i = 0;
      while (*p != '@')
	{
	  if (!fl && strchr ("~+-^=", *p))
	    {
	      if (strict_host)
		nw[i] = '?';
	      else
		i--;
	    }
	  else
	    nw[i] = *p;
	  fl++;
	  p++;
	  i++;
	}
      nw[i++] = '@';
      q++;
    }
  else
    {
      nw[0] = '*';
      nw[1] = '@';
      i = 2;
      q = s;
    }
  nw += i;
  e = NULL;
  if ((!(p = strchr (q, '.')) || !(e = strchr (p + 1, '.')))
      && !strchr (q, ':'))
    strcpy (nw, q);
  else
    {
      if (e == NULL)
	{
	  const char *mask_str;
	  f = strrchr (q, ':');
	  if (strchr (f, '.'))
	    {
	      f = strrchr (f, '.');
	      mask_str = ".*";
	    }
	  else
	    mask_str = ":*";
	  strncpy (nw, q, f - q);
	  nw += (f - q);
	  strcpy (nw, mask_str);
	}
      else
	{
	  for (f = e; *f; f++);
	  f--;
	  if (*f >= '0' && *f <= '9')
	    {
	      while (*f != '.')
		f--;
	      strncpy (nw, q, f - q);
	      nw += (f - q);
	      strcpy (nw, ".*");
	    }
	  else
	    {
	      const char *x = strchr (e + 1, '.');
	      if (!x)
		x = p;
	      else if (strchr (x + 1, '.'))
		x = e;
	      else if (strlen (x) == 3)
		x = p;
	      else
		x = e;
	      sprintf (nw, "*%s", x);
	    }
	}
    }
}
void
dumplots (int idx, const char *prefix, char *data)
{
  char *p = data, *q, *n, c;
  const int max_data_len = 500 - strlen (prefix);
  if (!*data)
    {
      dprintf (idx, "%s\n", prefix);
      return;
    }
  while (strlen (p) > max_data_len)
    {
      q = p + max_data_len;
      n = strchr (p, '\n');
      if (n && n < q)
	{
	  *n = 0;
	  dprintf (idx, "%s%s\n", prefix, p);
	  *n = '\n';
	  p = n + 1;
	}
      else
	{
	  while (*q != ' ' && q != p)
	    q--;
	  if (q == p)
	    q = p + max_data_len;
	  c = *q;
	  *q = 0;
	  dprintf (idx, "%s%s\n", prefix, p);
	  *q = c;
	  p = q;
	  if (c == ' ')
	    p++;
	}
    }
  n = strchr (p, '\n');
  while (n)
    {
      *n = 0;
      dprintf (idx, "%s%s\n", prefix, p);
      *n = '\n';
      p = n + 1;
      n = strchr (p, '\n');
    }
  if (*p)
    dprintf (idx, "%s%s\n", prefix, p);
}

void
daysago (time_t now, time_t then, char *out)
{
  if (now - then > 86400)
    {
      int days = (now - then) / 86400;
      sprintf (out, "%d day%s ago", days, (days == 1) ? "" : "s");
      return;
    }
  egg_strftime (out, 6, "%H:%M", localtime (&then));
}

void
days (time_t now, time_t then, char *out)
{
  if (now - then > 86400)
    {
      int days = (now - then) / 86400;
      sprintf (out, "in %d day%s", days, (days == 1) ? "" : "s");
      return;
    }
  egg_strftime (out, 9, "at %H:%M", localtime (&now));
}

void
daysdur (time_t now, time_t then, char *out)
{
  char s[81];
  int hrs, mins;
  if (now - then > 86400)
    {
      int days = (now - then) / 86400;
      sprintf (out, "for %d day%s", days, (days == 1) ? "" : "s");
      return;
    }
  strcpy (out, "for ");
  now -= then;
  hrs = (int) (now / 3600);
  mins = (int) ((now - (hrs * 3600)) / 60);
  sprintf (s, "%02d:%02d", hrs, mins);
  strcat (out, s);
} void
show_motd (int idx)
{
  dprintf (idx, STR ("Motd: "));
  if (CFG_MOTD.gdata && *(char *) CFG_MOTD.gdata)
    dprintf (idx, STR ("%s\n"), (char *) CFG_MOTD.gdata);
  else
    dprintf (idx, STR ("none\n"));
}

int
getting_users ()
{
  int i;
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_BOT) && (dcc[i].status & STAT_GETTING))
      return 1;
  return 0;
}

int
prand (int *seed, int range)
{
  long long i1;
  i1 = *seed;
  i1 = (i1 * 0x08088405 + 1) & 0xFFFFFFFF;
  *seed = i1;
  i1 = (i1 * range) >> 32;
  return i1;
}
void putlog
EGG_VARARGS_DEF (int, arg1)
{
  int i, type, tsl = 0, dohl = 0;
  char *format, *chname, s[LOGLINELEN], s1[256], *out, ct[81], *s2, stamp[34],
    buf2[LOGLINELEN];
  va_list va;
#ifdef HUB
  time_t now2 = time (NULL);
#endif
  struct tm *t;
  t = 0;
  type = EGG_VARARGS_START (int, arg1, va);
  chname = va_arg (va, char *);
  format = va_arg (va, char *);
  if ((chname[0] == '*'))
    dohl = 1;
#ifdef HUB
  if (shtime)
    {
      t = localtime (&now2);
      egg_strftime (stamp, sizeof (stamp) - 2, LOG_TS, t);
      strcat (stamp, " ");
      tsl = strlen (stamp);
    }
#endif
  out = s + tsl;
  egg_vsnprintf (out, LOGLINEMAX - tsl, format, va);
  out[LOGLINEMAX - tsl] = 0;
  if (keep_all_logs)
    {
      if (!logfile_suffix[0])
	egg_strftime (ct, 12, ".%d%b%Y", t);
      else
	{
	  egg_strftime (ct, 80, logfile_suffix, t);
	  ct[80] = 0;
	  s2 = ct;
	  while (s2[0])
	    {
	      if (s2[0] == ' ')
		s2[0] = '_';
	      s2++;
	    }
	}
    }
  if ((out[0]) && (shtime))
    {
      strncpy (s, stamp, tsl);
      out = s;
    }
  strcat (out, "\n");
  if (!use_stderr)
    {
      for (i = 0; i < max_logs; i++)
	{
	  if ((logs[i].filename != NULL) && (logs[i].mask & type)
	      && ((chname[0] == '@') || (chname[0] == '*')
		  || (logs[i].chname[0] == '*')
		  || (!rfc_casecmp (chname, logs[i].chname))))
	    {
	      if (logs[i].f == NULL)
		{
		  if (keep_all_logs)
		    {
		      egg_snprintf (s1, 256, "%s%s", logs[i].filename, ct);
		      logs[i].f = fopen (s1, "a+");
		    }
		  else
		    logs[i].f = fopen (logs[i].filename, "a+");
		}
	      if (logs[i].f != NULL)
		{
		  if (!egg_strcasecmp (out + tsl, logs[i].szlast))
		    logs[i].repeats++;
		  else
		    {
		      if (logs[i].repeats > 0)
			{
			  fprintf (logs[i].f, stamp);
			  fprintf (logs[i].f, MISC_LOGREPEAT,
				   logs[i].repeats);
			  logs[i].repeats = 0;
			}
		      fputs (out, logs[i].f);
		      strncpyz (logs[i].szlast, out + tsl, LOGLINEMAX);
		    }
		}
	    }
	}
    }
  if (dohl)
    {
      sprintf (buf2, "hl %d %s", type, out);
      botnet_send_zapf_broad (-1, botnetnick, NULL, buf2);
    }
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_CHAT) && (dcc[i].u.chat->con_flags & type))
      {
	if ((chname[0] == '@') || (chname[0] == '*')
	    || (dcc[i].u.chat->con_chan[0] == '*')
	    || (!rfc_casecmp (chname, dcc[i].u.chat->con_chan)))
	  dprintf (i, "%s", out);
      }
  if ((!backgrd) && (!con_chan) && (!term_z))
    dprintf (DP_STDOUT, "%s", out);
  else if ((type & LOG_MISC) && use_stderr)
    {
      if (shtime)
	out += tsl;
      dprintf (DP_STDERR, "%s", s);
    }
  va_end (va);
}

void
logsuffix_change (char *s)
{
  int i;
  char *s2 = logfile_suffix;
  debug0 ("Logfile suffix changed. Closing all open logs.");
  strcpy (logfile_suffix, s);
  while (s2[0])
    {
      if (s2[0] == ' ')
	s2[0] = '_';
      s2++;
    }
  for (i = 0; i < max_logs; i++)
    {
      if (logs[i].f)
	{
	  fflush (logs[i].f);
	  fclose (logs[i].f);
	  logs[i].f = NULL;
	}
    }
}
void
check_logsize ()
{
  struct stat ss;
  int i;
  char buf[1024];
  if (!keep_all_logs && max_logsize > 0)
    {
      for (i = 0; i < max_logs; i++)
	{
	  if (logs[i].filename)
	    {
	      if (stat (logs[i].filename, &ss) != 0)
		{
		  break;
		}
	      if ((ss.st_size >> 10) > max_logsize)
		{
		  if (logs[i].f)
		    {
		      putlog (LOG_MISC, "*", MISC_CLOGS, logs[i].filename,
			      ss.st_size);
		      fflush (logs[i].f);
		      fclose (logs[i].f);
		      logs[i].f = NULL;
		    }
		  egg_snprintf (buf, sizeof buf, "%s.yesterday",
				logs[i].filename);
		  buf[1023] = 0;
		  unlink (buf);
		  movefile (logs[i].filename, buf);
		}
	    }
	}
    }
}
void
flushlogs ()
{
  int i;
  if (!logs)
    return;
  for (i = 0; i < max_logs; i++)
    {
      if (logs[i].f != NULL)
	{
	  if ((logs[i].repeats > 0) && quick_logs)
	    {
	      char stamp[32];
	      egg_strftime (&stamp[0], 32, LOG_TS, localtime (&now));
	      fprintf (logs[i].f, "%s ", stamp);
	      fprintf (logs[i].f, MISC_LOGREPEAT, logs[i].repeats);
	      logs[i].repeats = 0;
	    }
	  fflush (logs[i].f);
	}
}} char *
extracthostname (char *hostmask)
{
  char *p = strrchr (hostmask, '@');
  return p ? p + 1 : "";
}

void
make_rand_str (char *s, int len)
{
  int j, r = 0;
  Context;
  for (j = 0; j < len; j++)
    {
      r = random ();
      Context;
      if (r % 3 == 0)
	s[j] = '0' + (random () % 10);
      else if (r % 3 == 1)
	s[j] = 'a' + (random () % 26);
      else
	s[j] = 'A' + (random () % 26);
    }
  s[len] = 0;
  Context;
}

int
oatoi (const char *octal)
{
  register int i;
  if (!*octal)
    return -1;
  for (i = 0; ((*octal >= '0') && (*octal <= '7')); octal++)
    i = (i * 8) + (*octal - '0');
  if (*octal)
    return -1;
  return i;
}

char *
str_escape (const char *str, const char div, const char mask)
{
  const int len = strlen (str);
  int buflen = (2 * len), blen = 0;
  char *buf = nmalloc (buflen + 1), *b = buf;
  const char *s;
  if (!buf)
    return NULL;
  for (s = str; *s; s++)
    {
      if ((buflen - blen) <= 3)
	{
	  buflen = (buflen * 2);
	  buf = nrealloc (buf, buflen + 1);
	  if (!buf)
	    return NULL;
	  b = buf + blen;
	}
      if (*s == div || *s == mask)
	{
	  sprintf (b, "%c%02x", mask, *s);
	  b += 3;
	  blen += 3;
	}
      else
	{
	  *(b++) = *s;
	  blen++;
	}
    }
  *b = 0;
  return buf;
}

char *
strchr_unescape (char *str, const char div, register const char esc_char)
{
  char buf[3];
  register char *s, *p;
  buf[3] = 0;
  for (s = p = str; *s; s++, p++)
    {
      if (*s == esc_char)
	{
	  buf[0] = s[1], buf[1] = s[2];
	  *p = (unsigned char) strtol (buf, NULL, 16);
	  s += 2;
	}
      else if (*s == div)
	{
	  *p = *s = 0;
	  return (s + 1);
	}
      else
	*p = *s;
    }
  *p = 0;
  return NULL;
}

void
str_unescape (char *str, register const char esc_char)
{
  (void) strchr_unescape (str, 0, esc_char);
} void
kill_bot (char *s1, char *s2)
{
#ifdef HUB
  write_userfile (-1);
#endif
  call_hook (HOOK_DIE);
  chatout ("*** %s\n", s1);
  botnet_send_chat (-1, botnetnick, s1);
  botnet_send_bye ();
  fatal (s2, 0);
} int

isupdatehub ()
{
#ifdef HUB
  struct userrec *buser;
  buser = get_user_by_handle (userlist, botnetnick);
  if ((buser) && (buser->flags & USER_UPDATEHUB))
    return 1;
  else
#endif
    return 0;
}

int
ischanhub ()
{
  struct userrec *buser;
  buser = get_user_by_handle (userlist, botnetnick);
  if ((buser) && (buser->flags & USER_CHANHUB))
    return 1;
  else
    return 0;
}

int
issechub ()
{
  struct userrec *buser;
  buser = get_user_by_handle (userlist, botnetnick);
  if ((buser) && (buser->flags & USER_SECHUB))
    return 1;
  else
    return 0;
}

#ifdef S_DCCPASS
int
check_cmd_pass (char *cmd, char *pass)
{
  struct cmd_pass *cp;
  for (cp = cmdpass; cp; cp = cp->next)
    if (!strcasecmp2 (cmd, cp->name))
      {
	char tmp[32];
	encrypt_pass (pass, tmp);
	if (!strcmp (tmp, cp->pass))
	  return 1;
	return 0;
      }
  return 0;
}

int
has_cmd_pass (char *cmd)
{
  struct cmd_pass *cp;
  for (cp = cmdpass; cp; cp = cp->next)
    if (!strcasecmp2 (cmd, cp->name))
      return 1;
  return 0;
}

void
set_cmd_pass (char *ln, int shareit)
{
  struct cmd_pass *cp;
  char *cmd;
  cmd = newsplit (&ln);
  for (cp = cmdpass; cp; cp = cp->next)
    if (!strcmp (cmd, cp->name))
      break;
  if (cp)
    if (ln[0])
      {
	strcpy (cp->pass, ln);
	if (shareit)
	  botnet_send_cmdpass (-1, cp->name, cp->pass);
      }
    else
      {
	if (cp == cmdpass)
	  cmdpass = cp->next;
	else
	  {
	    struct cmd_pass *cp2;
	    cp2 = cmdpass;
	    while (cp2->next != cp)
	      cp2 = cp2->next;
	    cp2->next = cp->next;
	  }
	if (shareit)
	  botnet_send_cmdpass (-1, cp->name, "");
	nfree (cp->name);
	nfree (cp);
      }
  else if (ln[0])
    {
      cp = nmalloc (sizeof (struct cmd_pass));
      cp->next = cmdpass;
      cmdpass = cp;
      cp->name = nmalloc (strlen (cmd) + 1);
      strcpy (cp->name, cmd);
      strcpy (cp->pass, ln);
      if (shareit)
	botnet_send_cmdpass (-1, cp->name, cp->pass);
    }
}
#endif
#ifdef S_LASTCHECK
char last_buf[128] = "";
#endif
void
check_last ()
{
#ifdef S_LASTCHECK
  char user[20];
  struct passwd *pw;
  Context;
  pw = getpwuid (geteuid ());
  Context;
  if (!pw)
    return;
  strncpy0 (user, pw->pw_name ? pw->pw_name : "", sizeof (user));
  if (user[0])
    {
      char *out;
      char buf[50];
      sprintf (buf, STR ("last %s"), user);
      if (shell_exec (buf, NULL, &out, NULL))
	{
	  if (out)
	    {
	      char *p;
	      p = strchr (out, '\n');
	      if (p)
		*p = 0;
	      if (strlen (out) > 10)
		{
		  if (last_buf[0])
		    {
		      if (strncmp (last_buf, out, sizeof (last_buf)))
			{
			  char wrk[16384];
			  sprintf (wrk, STR ("Login: %s"), out);
			  detected (DETECT_LOGIN, wrk);
			}
		    }
		  strncpy0 (last_buf, out, sizeof (last_buf));
		}
	      nfree (out);
	    }
	}
    }
#endif
}
struct cfg_entry *
check_can_set_cfg (char *target, char *entryname)
{
  int i;
  struct userrec *u;
  struct cfg_entry *entry = NULL;
  for (i = 0; i < cfg_count; i++)
    if (!strcmp (cfg[i]->name, entryname))
      {
	entry = cfg[i];
	break;
      }
  if (!entry)
    return 0;
  if (target)
    {
      if (!(entry->flags & CFGF_LOCAL))
	return 0;
      if (!(u = get_user_by_handle (userlist, target)))
	return 0;
      if (!(u->flags & USER_BOT))
	return 0;
    }
  else
    {
      if (!(entry->flags & CFGF_GLOBAL))
	return 0;
    }
  return entry;
}

void
set_cfg_str (char *target, char *entryname, char *data)
{
  struct cfg_entry *entry;
  int free = 0;
  if (!(entry = check_can_set_cfg (target, entryname)))
    return;
  if (data && !strcmp (data, "-"))
    data = NULL;
  if (data && (strlen (data) >= 1024))
    data[1023] = 0;
  if (target)
    {
      struct userrec *u = get_user_by_handle (userlist, target);
      struct xtra_key *xk;
      char *olddata = entry->ldata;
      if (u && !strcmp (botnetnick, u->handle))
	{
	  if (data)
	    {
	      entry->ldata = nmalloc (strlen (data) + 1);
	      strcpy (entry->ldata, data);
	    }
	  else
	    entry->ldata = NULL;
	  if (entry->localchanged)
	    {
	      int valid = 1;
	      entry->localchanged (entry, olddata, &valid);
	      if (!valid)
		{
		  if (entry->ldata)
		    nfree (entry->ldata);
		  entry->ldata = olddata;
		  data = olddata;
		  olddata = NULL;
		}
	    }
	}
      xk = nmalloc (sizeof (struct xtra_key));
      egg_bzero (xk, sizeof (struct xtra_key));
      xk->key = nmalloc (strlen (entry->name) + 1);
      strcpy (xk->key, entry->name);
      if (data)
	{
	  xk->data = nmalloc (strlen (data) + 1);
	  strcpy (xk->data, data);
	}
      set_user (&USERENTRY_CONFIG, u, xk);
      if (olddata)
	nfree (olddata);
    }
  else
    {
      char *olddata = entry->gdata;
      if (data)
	{
	  free = 1;
	  entry->gdata = nmalloc (strlen (data) + 1);
	  strcpy (entry->gdata, data);
	}
      else
	entry->gdata = NULL;
      if (entry->globalchanged)
	{
	  int valid = 1;
	  entry->globalchanged (entry, olddata, &valid);
	  if (!valid)
	    {
	      if (entry->gdata)
		nfree (entry->gdata);
	      entry->gdata = olddata;
	      olddata = NULL;
	    }
	}
      if (!cfg_noshare)
	botnet_send_cfg_broad (-1, entry);
      if (olddata)
	nfree (olddata);
    }
}
void
userfile_cfg_line (char *ln)
{
  char *name;
  int i;
  struct cfg_entry *cfgent = NULL;
  name = newsplit (&ln);
  for (i = 0; !cfgent && (i < cfg_count); i++)
    if (!strcmp (cfg[i]->name, name))
      cfgent = cfg[i];
  if (cfgent)
    {
      set_cfg_str (NULL, cfgent->name, ln[0] ? ln : NULL);
    }
  else
    putlog (LOG_ERRORS, "*", STR ("Unrecognized config entry %s in userfile"),
	    name);
}

void
got_config_share (int idx, char *ln)
{
  char *name;
  int i;
  struct cfg_entry *cfgent = NULL;
  cfg_noshare++;
  name = newsplit (&ln);
  for (i = 0; !cfgent && (i < cfg_count); i++)
    if (!strcmp (cfg[i]->name, name))
      cfgent = cfg[i];
  if (cfgent)
    {
      set_cfg_str (NULL, cfgent->name, ln[0] ? ln : NULL);
      botnet_send_cfg_broad (idx, cfgent);
    }
  else
    putlog (LOG_ERRORS, "*", STR ("Unrecognized config entry %s in userfile"),
	    name);
  cfg_noshare--;
}

void
add_cfg (struct cfg_entry *entry)
{
  cfg = (void *) user_realloc (cfg, sizeof (void *) * (cfg_count + 1));
  cfg[cfg_count] = entry;
  cfg_count++;
  entry->ldata = NULL;
  entry->gdata = NULL;
} void

trigger_cfg_changed ()
{
  int i;
  struct userrec *u;
  struct xtra_key *xk;
  u = get_user_by_handle (userlist, botnetnick);
  for (i = 0; i < cfg_count; i++)
    {
      if (cfg[i]->flags & CFGF_LOCAL)
	{
	  xk = get_user (&USERENTRY_CONFIG, u);
	  while (xk && strcmp (xk->key, cfg[i]->name))
	    xk = xk->next;
	  if (xk)
	    {
	      putlog (LOG_DEBUG, "*", STR ("trigger_cfg_changed for %s"),
		      cfg[i]->name ? cfg[i]->name : "(null)");
	      if (!strcmp (cfg[i]->name, xk->key ? xk->key : ""))
		{
		  set_cfg_str (botnetnick, cfg[i]->name, xk->data);
		}
	    }
	}
    }
}
int
shell_exec (char *cmdline, char *input, char **output, char **erroutput)
{
  FILE *inpFile, *outFile, *errFile;
  char tmpfile[161];
  int x, fd;
  if (!cmdline)
    return 0;
  sprintf (tmpfile, STR ("%s.in-XXXXXX"), tempdir);
  if ((fd = mkstemp (tmpfile)) == -1 || (inpFile = fdopen (fd, "w+")) == NULL)
    {
      if (fd != -1)
	{
	  unlink (tmpfile);
	  close (fd);
	}
      putlog (LOG_ERRORS, "*", STR ("exec: Couldn't open %s"), tmpfile);
      return 0;
    }
  unlink (tmpfile);
  if (input)
    {
      if (fwrite (input, 1, strlen (input), inpFile) != strlen (input))
	{
	  fclose (inpFile);
	  putlog (LOG_ERRORS, "*", STR ("exec: Couldn't write to %s"),
		  tmpfile);
	  return 0;
	}
      fseek (inpFile, 0, SEEK_SET);
    }
  unlink (tmpfile);
  sprintf (tmpfile, STR ("%s.err-XXXXXX"), tempdir);
  if ((fd = mkstemp (tmpfile)) == -1 || (errFile = fdopen (fd, "w+")) == NULL)
    {
      if (fd != -1)
	{
	  unlink (tmpfile);
	  close (fd);
	}
      putlog (LOG_ERRORS, "*", STR ("exec: Couldn't open %s"), tmpfile);
      return 0;
    }
  unlink (tmpfile);
  sprintf (tmpfile, STR ("%s.out-XXXXXX"), tempdir);
  if ((fd = mkstemp (tmpfile)) == -1 || (outFile = fdopen (fd, "w+")) == NULL)
    {
      if (fd != -1)
	{
	  unlink (tmpfile);
	  close (fd);
	}
      putlog (LOG_ERRORS, "*", STR ("exec: Couldn't open %s"), tmpfile);
      return 0;
    }
  unlink (tmpfile);
  x = fork ();
  if (x == -1)
    {
      putlog (LOG_ERRORS, "*", STR ("exec: fork() failed"));
      fclose (inpFile);
      fclose (errFile);
      fclose (outFile);
      return 0;
    }
  if (x)
    {
      int st = 0;
      waitpid (x, &st, 0);
      fclose (inpFile);
      fflush (outFile);
      fflush (errFile);
      if (erroutput)
	{
	  char *buf;
	  int fs;
	  fseek (errFile, 0, SEEK_END);
	  fs = ftell (errFile);
	  if (fs == 0)
	    {
	      (*erroutput) = NULL;
	    }
	  else
	    {
	      buf = nmalloc (fs + 1);
	      fseek (errFile, 0, SEEK_SET);
	      fread (buf, 1, fs, errFile);
	      buf[fs] = 0;
	      (*erroutput) = buf;
	    }
	}
      fclose (errFile);
      if (output)
	{
	  char *buf;
	  int fs;
	  fseek (outFile, 0, SEEK_END);
	  fs = ftell (outFile);
	  if (fs == 0)
	    {
	      (*output) = NULL;
	    }
	  else
	    {
	      buf = nmalloc (fs + 1);
	      fseek (outFile, 0, SEEK_SET);
	      fread (buf, 1, fs, outFile);
	      buf[fs] = 0;
	      (*output) = buf;
	    }
	}
      fclose (outFile);
      return 1;
    }
  else
    {
      int ind, outd, errd;
      char *argv[4];
      ind = fileno (inpFile);
      outd = fileno (outFile);
      errd = fileno (errFile);
      if (dup2 (ind, STDIN_FILENO) == (-1))
	{
	  exit (1);
	}
      if (dup2 (outd, STDOUT_FILENO) == (-1))
	{
	  exit (1);
	}
      if (dup2 (errd, STDERR_FILENO) == (-1))
	{
	  exit (1);
	}
      argv[0] = STR ("/bin/sh");
      argv[1] = STR ("-c");
      argv[2] = cmdline;
      argv[3] = NULL;
      execvp (argv[0], &argv[0]);
      exit (1);
    }
}
int ucnt = 0;
static void
updatelocal (void)
{
#ifdef LEAF
  module_entry *me;
#endif
  Context;
  if (ucnt < 300)
    {
      ucnt++;
      return;
    }
  del_hook (HOOK_SECONDLY, (Function) updatelocal);
  ucnt = 0;
#ifdef LEAF
  if ((me = module_find ("server", 0, 0)))
    {
      Function *func = me->funcs;
      (func[SERVER_NUKESERVER]) ("Updating...");
    }
#endif
  botnet_send_chat (-1, botnetnick, "Updating...");
  botnet_send_bye ();
  fatal ("Updating...", 1);
  usleep (2000 * 500);
  bg_send_quit (BG_ABORT);
  unlink (pid_file);
  system (binname);
  exit (0);
}

int
updatebin (int idx, char *par, int autoi)
{
  char *path = NULL, *newbin;
  char buf[2048], old[1024];
  struct stat sb;
  int i;
#ifdef LEAF
  module_entry *me;
#endif
  path = newsplit (&par);
  par = path;
  if (!par[0])
    {
      if (idx)
	dprintf (idx, "Not enough parameters.\n");
      return 1;
    }
  path = nmalloc (strlen (binname) + strlen (par));
  strcpy (path, binname);
  newbin = strrchr (path, '/');
  if (!newbin)
    {
      nfree (path);
      if (idx)
	dprintf (idx, "Don't know current binary name\n");
      return 1;
    }
  newbin++;
  if (strchr (par, '/'))
    {
      *newbin = 0;
      if (idx)
	dprintf (idx,
		 "New binary must be in %s and name must be specified without path information\n",
		 path);
      nfree (path);
      return 1;
    }
  strcpy (newbin, par);
  if (!strcmp (path, binname))
    {
      nfree (path);
      if (idx)
	dprintf (idx, "Can't update with the current binary\n");
      return 1;
    }
  if (stat (path, &sb))
    {
      if (idx)
	dprintf (idx, "%s can't be accessed\n", path);
      nfree (path);
      return 1;
    }
  if (chmod (path, S_IRUSR | S_IWUSR | S_IXUSR))
    {
      if (idx)
	dprintf (idx, "Can't set mode 0600 on %s\n", path);
      nfree (path);
      return 1;
    }
  sprintf (old, "%s.bin.old", tempdir);
  copyfile (binname, old);
  if (movefile (path, binname))
    {
      if (idx)
	dprintf (idx, "Can't rename %s to %s\n", path, binname);
      nfree (path);
      return 1;
    }
  sprintf (buf, "%s", binname);
#ifdef LEAF
  if (localhub)
    {
      sprintf (buf, "%s -P %d", buf, getpid ());
    }
#endif
#ifdef LEAF
  if (!autoi && !localhub)
#endif
    unlink (pid_file);
#ifdef HUB
  listen_all (my_port, 1);
#endif
  i = system (buf);
  if (i == -1 || i == 1)
    {
      if (idx)
	dprintf (idx, "Couldn't restart new binary (error %d)\n", i);
      putlog (LOG_MISC, "*", "Couldn't restart new binary (error %d)\n", i);
      return i;
    }
  else
    {
#ifdef LEAF
      if (!autoi && !localhub)
	{
	  if ((me = module_find ("server", 0, 0)))
	    {
	      Function *func = me->funcs;
	      (func[SERVER_NUKESERVER]) ("Updating...");
	    }
#endif
	  if (idx)
	    dprintf (idx, "Updating...bye\n");
	  putlog (LOG_MISC, "*", "Updating...\n");
	  botnet_send_chat (-1, botnetnick, "Updating...");
	  botnet_send_bye ();
	  fatal ("Updating...", 1);
	  usleep (2000 * 500);
	  bg_send_quit (BG_ABORT);
	  exit (0);
#ifdef LEAF
	}
      else
	{
	  if (localhub && autoi)
	    {
	      add_hook (HOOK_SECONDLY, (Function) updatelocal);
	      return 0;
	    }
	}
#endif
    }
  return 2;
}

void
EncryptFile (char *infile, char *outfile)
{
  char buf[8192];
  FILE *f, *f2;
  Context;
  f = fopen (infile, "r");
  if (!f)
    return;
  f2 = fopen (outfile, "w");
  if (!f2)
    return;
  Context;
  while (fscanf (f, "%[^\n]\n", buf) != EOF)
    {
      Context;
      lfprintf (f2, "%s\n", buf);
      Context;
    }
  fclose (f);
  fclose (f2);
}

void
DecryptFile (char *infile, char *outfile)
{
  char buf[8192], *temps;
  FILE *f, *f2;
  f = fopen (infile, "r");
  if (!f)
    return;
  f2 = fopen (outfile, "w");
  if (!f2)
    return;
  while (fscanf (f, "%[^\n]\n", buf) != EOF)
    {
      temps = (char *) decrypt_string (netpass, decryptit (buf));
      fprintf (f2, "%s\n", temps);
      nfree (temps);
    } fclose (f);
  fclose (f2);
} int
bot_aggressive_to (struct userrec *u)
{
  char mypval[20], botpval[20];
  link_pref_val (u, botpval);
  link_pref_val (get_user_by_handle (userlist, botnetnick), mypval);
  if (strcmp (mypval, botpval) < 0)
    return 1;
  else
    return 0;
}

void
detected (int code, char *msg)
{
#ifdef LEAF
  module_entry *me;
#endif
  char *p = NULL;
  char tmp[512];
  struct userrec *u;
  struct flag_record fr = { FR_GLOBAL, 0, 0 };
  int act;
  u = get_user_by_handle (userlist, botnetnick);
#ifdef S_LASTCHECK
  if (code == DETECT_LOGIN)
    p =
      (char *) (CFG_LOGIN.ldata ? CFG_LOGIN.
		ldata : (CFG_LOGIN.gdata ? CFG_LOGIN.gdata : NULL));
#endif
#ifdef S_ANTITRACE
  if (code == DETECT_TRACE)
    p =
      (char *) (CFG_TRACE.ldata ? CFG_TRACE.
		ldata : (CFG_TRACE.gdata ? CFG_TRACE.gdata : NULL));
#endif
#ifdef S_PROMISC
  if (code == DETECT_PROMISC)
    p =
      (char *) (CFG_PROMISC.ldata ? CFG_PROMISC.
		ldata : (CFG_PROMISC.gdata ? CFG_PROMISC.gdata : NULL));
#endif
#ifdef S_PROCESSCHECK
  if (code == DETECT_PROCESS)
    p =
      (char *) (CFG_BADPROCESS.ldata ? CFG_BADPROCESS.
		ldata : (CFG_BADPROCESS.gdata ? CFG_BADPROCESS.gdata : NULL));
#endif
  if (!p)
    act = DET_WARN;
  else if (!strcmp (p, STR ("die")))
    act = DET_DIE;
  else if (!strcmp (p, STR ("reject")))
    act = DET_REJECT;
  else if (!strcmp (p, STR ("suicide")))
    act = DET_SUICIDE;
  else if (!strcmp (p, STR ("nocheck")))
    act = DET_NOCHECK;
  else if (!strcmp (p, STR ("ignore")))
    act = DET_IGNORE;
  else
    act = DET_WARN;
  switch (act)
    {
    case DET_IGNORE:
      break;
    case DET_WARN:
      putlog (LOG_WARN, "*", msg);
      break;
    case DET_REJECT:
      putlog (LOG_WARN, "*", STR ("Setting myself +d: %s"), msg);
      sprintf (tmp, STR ("+d: %s"), msg);
      set_user (&USERENTRY_COMMENT, u, tmp);
      get_user_flagrec (u, &fr, 0);
      fr.global = USER_DEOP | USER_BOT;
      set_user_flagrec (u, &fr, 0);
      sleep (1);
      break;
    case DET_DIE:
      putlog (LOG_WARN, "*", STR ("Dying: %s"), msg);
      sprintf (tmp, STR ("Dying: %s"), msg);
      set_user (&USERENTRY_COMMENT, u, tmp);
#ifdef LEAF
      if ((me = module_find ("server", 0, 0)))
	{
	  Function *func = me->funcs;
	  (func[SERVER_NUKESERVER]) ("BBL");
	}
#endif
      sleep (1);
      fatal (msg, 0);
      break;
    case DET_SUICIDE:
      putlog (LOG_WARN, "*", STR ("Comitting suicide: %s"), msg);
      sprintf (tmp, STR ("Suicide: %s"), msg);
      set_user (&USERENTRY_COMMENT, u, tmp);
#ifdef LEAF
      if ((me = module_find ("server", 0, 0)))
	{
	  Function *func = me->funcs;
	  (func[SERVER_NUKESERVER]) ("HARAKIRI!!");
	}
#endif
      sleep (1);
      unlink (binname);
#ifdef HUB
      unlink (userfile);
      sprintf (tmp, STR ("%s~"), userfile);
      unlink (tmp);
#endif
      fatal (msg, 0);
      break;
    case DET_NOCHECK:
      break;
    }
}
char *
kickreason (int kind)
{
  int r;
  r = random ();
  switch (kind)
    {
    case KICK_BANNED:
      switch (r % 6)
	{
	case 0:
	  return STR ("bye");
	case 1:
	  return STR ("banned");
	case 2:
	  return STR ("bummer");
	case 3:
	  return STR ("go away");
	case 4:
	  return STR ("cya around looser");
	case 5:
	  return STR ("unwanted!");
	}
    case KICK_KUSER:
      switch (r % 4)
	{
	case 0:
	  return STR ("not wanted");
	case 1:
	  return STR ("something tells me you're annoying");
	case 2:
	  return STR ("don't bug me looser");
	case 3:
	  return STR ("creep");
	}
    case KICK_KICKBAN:
      switch (r % 4)
	{
	case 0:
	  return STR ("gone");
	case 1:
	  return STR ("stupid");
	case 2:
	  return STR ("looser");
	case 3:
	  return STR ("...");
	}
    case KICK_MASSDEOP:
      switch (r % 8)
	{
	case 0:
	  return STR ("spammer!");
	case 1:
	  return STR ("easy on the modes now");
	case 2:
	  return STR ("mode this");
	case 3:
	  return STR ("nice try");
	case 4:
	  return STR ("really?");
	case 5:
	  return STR ("mIRC sux for mdop kiddo");
	case 6:
	  return STR ("scary... really scary...");
	case 7:
	  return STR ("you lost the game!");
	}
    case KICK_BADOP:
      switch (r % 5)
	{
	case 0:
	  return STR ("neat...");
	case 1:
	  return STR ("oh, no you don't. go away.");
	case 2:
	  return STR ("didn't you forget something now?");
	case 3:
	  return STR ("no");
	case 4:
	  return STR ("hijack this");
	}
    case KICK_BADOPPED:
      switch (r % 5)
	{
	case 0:
	  return STR ("buggar off kid");
	case 1:
	  return STR ("asl?");
	case 2:
	  return STR ("whoa... what a hacker... skills!");
	case 3:
	  return STR ("yes! yes! yes! hit me baby one more time!");
	case 4:
	  return
	    STR
	    ("with your skills, you're better off jacking off than hijacking");
	}
    case KICK_MANUALOP:
      switch (r % 6)
	{
	case 0:
	  return STR ("naughty kid");
	case 1:
	  return STR ("didn't someone tell you that is bad?");
	case 2:
	  return STR ("want perm?");
	case 3:
	  return STR ("see how much good that did you?");
	case 4:
	  return STR ("not a smart move...");
	case 5:
	  return STR ("jackass!");
	}
    case KICK_MANUALOPPED:
      switch (r % 8)
	{
	case 0:
	  return STR ("your pal got mean friends. like me.");
	case 1:
	  return STR ("uhh now.. don't wake me up...");
	case 2:
	  return STR ("hi hun. missed me?");
	case 3:
	  return STR ("spammer! die!");
	case 4:
	  return STR ("boo!");
	case 5:
	  return STR ("that @ was useful, don't ya think?");
	case 6:
	  return STR ("not in my book");
	case 7:
	  return STR ("lol, really?");
	}
    case KICK_CLOSED:
      switch (r % 17)
	{
	case 0:
	  return STR ("locked");
	case 1:
	  return STR ("later");
	case 2:
	  return STR ("closed for now");
	case 3:
	  return
	    STR ("sorry, but it's getting late, locking channel. cya around");
	case 4:
	  return STR ("better safe than sorry");
	case 5:
	  return STR ("cleanup, come back later");
	case 6:
	  return STR ("this channel is closed");
	case 7:
	  return STR ("shutting down for now");
	case 8:
	  return STR ("lockdown");
	case 9:
	  return STR ("reopening later");
	case 10:
	  return STR ("not for the public atm");
	case 11:
	  return STR ("private channel for now");
	case 12:
	  return STR ("might reopen soon, might reopen later");
	case 13:
	  return STR ("you're not supposed to be here right now");
	case 14:
	  return STR ("sorry, closed");
	case 15:
	  return STR ("try us later, atm we're locked down");
	case 16:
	  return STR ("closed. try tomorrow");
	}
    case KICK_FLOOD:
      switch (r % 7)
	{
	case 0:
	  return STR ("so much bullshit in such a short time. amazing.");
	case 1:
	  return STR ("slow down. i'm trying to read here.");
	case 2:
	  return STR ("uhm... you actually think irc is for talking?");
	case 3:
	  return STR ("talk talk talk");
	case 4:
	  return STR ("blabbering are we?");
	case 5:
	  return STR ("... and i don't even like you!");
	case 6:
	  return STR ("and you're outa here...");
	}
    case KICK_NICKFLOOD:
      switch (r % 7)
	{
	case 0:
	  return STR ("make up your mind?");
	case 1:
	  return STR ("be schizofrenic elsewhere");
	case 2:
	  return STR ("I'm loosing track of you... not!");
	case 3:
	  return STR ("that is REALLY annoying");
	case 4:
	  return STR ("try this: /NICK looser");
	case 5:
	  return STR ("playing hide 'n' seek?");
	case 6:
	  return STR ("gotcha!");
	}
    case KICK_KICKFLOOD:
      switch (r % 6)
	{
	case 0:
	  return STR ("easier to just leave if you wan't to be alone");
	case 1:
	  return STR ("cool down");
	case 2:
	  return STR ("don't be so damned aggressive. that's my job.");
	case 3:
	  return STR ("kicking's fun, isn't it?");
	case 4:
	  return STR ("what's the rush?");
	case 5:
	  return STR ("next time you do that, i'll kick you again");
	}
    case KICK_BOGUSUSERNAME:
      return STR ("bogus username");
    case KICK_MEAN:
      switch (r % 11)
	{
	case 0:
	  return STR ("hey! that wasn't very nice!");
	case 1:
	  return STR ("don't fuck with my pals");
	case 2:
	  return STR ("meanie!");
	case 3:
	  return STR ("I can be a bitch too...");
	case 4:
	  return STR ("leave the bots alone, will ya?");
	case 5:
	  return STR ("not very clever");
	case 6:
	  return STR ("watch it");
	case 7:
	  return STR ("fuck off");
	case 8:
	  return STR ("easy now. that's a friend.");
	case 9:
	  return STR ("abuse of power. leave that to me, will ya?");
	case 10:
	  return
	    STR
	    ("there as some things you cannot do, and that was one of them...");
	}
    case KICK_BOGUSKEY:
      return STR ("I have a really hard time reading that key");
    default:
      return "OMFG@YUO";
    }
}
char kickprefix[20] = "";
char bankickprefix[20] = "";
void
makeplaincookie (char *chname, char *nick, char *buf)
{
  char work[256], work2[256];
  int i, n;
  sprintf (work, STR ("%010li"), (now + timesync));
  strcpy (buf, (char *) &work[4]);
  work[0] = 0;
  if (strlen (nick) < 5)
    while (strlen (work) + strlen (nick) < 5)
      strcat (work, " ");
  else
    strcpy (work, (char *) &nick[strlen (nick) - 5]);
  strcat (buf, work);
  n = 3;
  for (i = strlen (chname) - 1; (i >= 0) && (n >= 0); i--)
    if (((unsigned char) chname[i] < 128) && ((unsigned char) chname[i] > 32))
      {
	work2[n] = tolower (chname[i]);
	n--;
      }
  while (n >= 0)
    work2[n--] = ' ';
  work2[4] = 0;
  strcat (buf, work2);
}
