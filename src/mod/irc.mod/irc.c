#ifdef LEAF
#define MODULE_NAME "irc"
#define MAKING_IRC
#include "src/mod/module.h"
#include "irc.h"
#include "server.mod/server.h"
#undef serv
#include "channels.mod/channels.h"
#include "blowfish.mod/blowfish.h"
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif
#define op_bots (CFG_OPBOTS.gdata ? atoi(CFG_OPBOTS.gdata) : 1)
#define in_bots (CFG_INBOTS.gdata ? atoi(CFG_INBOTS.gdata) : 1)
#define lag_threshold (CFG_LAGTHRESHOLD.gdata ? atoi(CFG_LAGTHRESHOLD.gdata) : 15)
#define opreq_count (CFG_OPREQUESTS.gdata ? atoi( CFG_OPREQUESTS.gdata ) : 2)
#define opreq_seconds (CFG_OPREQUESTS.gdata ? atoi( strchr(CFG_OPREQUESTS.gdata, ':') + 1 ) : 5)
#define PRIO_DEOP 1
#define PRIO_KICK 2
int host_synced = 0;
struct cfg_entry CFG_OPBOTS, CFG_INBOTS, CFG_LAGTHRESHOLD, CFG_OPTIMESLACK,
#ifdef G_AUTOLOCK
  CFG_KILLTHRESHOLD, CFG_LOCKTHRESHOLD, CFG_FIGHTTHRESHOLD,
#endif
  CFG_OPREQUESTS;
static p_tcl_bind_list H_topc, H_splt, H_sign, H_rejn, H_part, H_pub, H_pubm;
static p_tcl_bind_list H_nick, H_mode, H_kick, H_join, H_need;
static Function *global = NULL, *channels_funcs = NULL, *server_funcs =
  NULL, *encryption_funcs = NULL;
static char kickprefix[20] = "";
static char bankickprefix[20] = "";
static int ctcp_mode;
static int net_type;
static int strict_host;
static int wait_split = 300;
static int max_bans = 25;
static int max_exempts = 20;
static int max_invites = 20;
static int max_modes = 20;
static int bounce_bans = 0;
static int bounce_exempts = 0;
static int bounce_invites = 0;
static int bounce_modes = 0;
static int learn_users = 0;
static int wait_info = 180;
static int invite_key = 1;
static int no_chanrec_info = 0;
static int modesperline = 4;
static int mode_buf_len = 200;
static int use_354 = 0;
static int kick_method = 1;
static int kick_fun = 0;
static int ban_fun = 1;
static int keepnick = 1;
static int prevent_mixing = 1;
static int rfc_compliant = 1;
static int include_lk = 1;
#include "chan.c"
#include "mode.c"
#include "cmdsirc.c"
#include "msgcmds.c"
#include "tclirc.c"
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

void
makeopline (struct chanset_t *chan, char *nick, char *buf)
{
  char plaincookie[20], enccookie[48], *p, nck[20], key[200];
  memberlist *m;
  m = ismember (chan, nick);
  if (m)
    strcpy (nck, m->nick);
  else
    strcpy (nck, nick);
  makeplaincookie (chan->name, nck, plaincookie);
  strcpy (key, botname);
  strcat (key, netpass);
  p = encrypt_string (key, plaincookie);
  strcpy (enccookie, p);
  nfree (p);
  p = enccookie + strlen (enccookie) - 1;
  while (*p == '.')
    *p-- = 0;
  sprintf (buf, STR ("MODE %s +o-b %s *!*@[%s]\n"), chan->name, nck,
	   enccookie);
}

void
getin_request (char *botnick, char *code, char *par)
{
  char *tmp, *chname, *nck = NULL, *hst = NULL, *p, *p2, *p3;
  struct chanset_t *chan;
  memberlist *mem = NULL;
  struct userrec *user;
  char nick[33];
  char host[256];
  char s[256], s2[16];
  int lim, curlim, sendi = 0;
  struct maskrec **mr, *tmr;
  struct maskstruct *b;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0 };
  if (!server_online)
    return;
  tmp = nmalloc (strlen (par) + 1);
  strcpy (tmp, par);
  chname = strchr (tmp, ' ');
  if (!chname)
    {
      nfree (tmp);
      return;
    }
  *chname++ = 0;
  nck = strchr (chname, ' ');
  if (nck)
    *nck++ = 0;
  hst = strchr (nck, ' ');
  if (hst)
    *hst++ = 0;
  if (chname && chname[0])
    chan = findchan_by_dname (chname);
  else
    return;
  if (!chan)
    {
      putlog (LOG_GETIN, "*",
	      STR ("getin req from %s for %s which is not a valid channel!"),
	      botnick, chname);
      return;
    }
  if (!ismember (chan, botname))
    {
      putlog (LOG_GETIN, "*",
	      STR ("getin req from %s for %s - I'm not on %s!"), botnick,
	      chname, chname);
      return;
    }
  user = get_user_by_handle (userlist, botnick);
  if (nck)
    {
      Context;
      mem = chan ? ismember (chan, nck) : NULL;
      strncpy0 (nick, nck, sizeof (nick));
    }
  else
    nick[0] = 0;
  if (hst)
    {
      strncpy0 (host, hst, sizeof (host));
    }
  else
    host[0] = 0;
  if (par[0] == 'o')
    {
      if (!nick[0])
	{
	  putlog (LOG_GETIN, "*",
		  STR
		  ("opreq from %s/??? on %s - No nick specified - SHOULD NOT HAPPEN"),
		  botnick, chname);
	  nfree (tmp);
	  return;
	}
      if (!chan)
	{
	  putlog (LOG_GETIN, "*",
		  STR ("opreq from %s/%s on %s - Channel %s doesn't exist"),
		  botnick, nick, chname, chname);
	  nfree (tmp);
	  return;
	}
      nfree (tmp);
      if (!mem)
	{
	  putlog (LOG_GETIN, "*",
		  STR ("opreq from %s/%s on %s - %s isn't on %s"), botnick,
		  nick, chan->dname, nick, chan->dname);
	  return;
	}
      if (!user)
	{
	  putlog (LOG_GETIN, "*",
		  STR
		  ("opreq from %s/%s on %s - No user called %s in userlist"),
		  botnick, nick, chan->dname, botnick);
	  return;
	}
      if (mem->user != user)
	{
#ifdef S_AUTOHOSTADD
	  memberlist *m;
	  m = ismember (chan, nick);
	  if (m)
	    {
	      char s[1024];
	      sprintf (s, STR ("*!*%s"), m->userhost);
	      addhost_by_handle (botnick, s);
	      putlog (LOG_GETIN, "*",
		      STR
		      ("opreq from %s/%s on %s - %s doesn't match %s Adding new host"),
		      botnick, nick, chan->dname, nick, botnick);
	    }
	  else
	    {
	      putlog (LOG_GETIN, "*",
		      STR
		      ("opreq from %s/%s on %s - %s doesn't match %s and is not in chan so not adding host."),
		      botnick, nick, chan->dname, nick, botnick);
	    }
	  return;
#endif
	  putlog (LOG_GETIN, "*",
		  STR ("opreq from %s/%s on %s - %s doesn't match %s"),
		  botnick, nick, chan->dname, nick, botnick);
	  return;
	}
      get_user_flagrec (user, &fr, NULL);
      if (!glob_op (fr))
	{
	  putlog (LOG_GETIN, "*",
		  STR ("opreq from %s/%s on %s - %s isn't global +o"),
		  botnick, nick, chan->dname, botnick);
	  return;
	}
      if (chan_hasop (mem))
	{
	  putlog (LOG_GETIN, "*",
		  STR ("opreq from %s/%s on %s - %s already has ops"),
		  botnick, nick, chan->dname, nick);
	  return;
	}
      if (chan_issplit (mem))
	{
	  putlog (LOG_GETIN, "*",
		  STR ("opreq from %s/%s on %s - %s is split"), botnick, nick,
		  chan->dname, nick);
	  return;
	}
      if (!me_op (chan))
	{
	  putlog (LOG_GETIN, "*",
		  STR ("opreq from %s/%s on %s - I haven't got ops"), botnick,
		  nick, chan->dname);
	  return;
	}
      if (chan_sentop (mem))
	{
	  putlog (LOG_GETIN, "*",
		  STR ("opreq from %s/%s on %s - Already sent a +o"), botnick,
		  nick, chan->dname);
	  return;
	}
      if (server_lag > lag_threshold)
	{
	  putlog (LOG_GETIN, "*",
		  STR ("opreq from %s/%s on %s - I'm too lagged"), botnick,
		  nick, chan->dname);
	  return;
	}
      if (getting_users ())
	{
	  putlog (LOG_GETIN, "*",
		  STR
		  ("opreq from %s/%s on %s - I'm getting userlist right now"),
		  botnick, nick, chan->dname);
	  return;
	}
      if (channel_fastop (chan) || channel_take (chan) || 0)
	{
	  add_mode (chan, '+', 'o', nick);
	}
      else
	{
	  tmp = nmalloc (strlen (chan->dname) + 200);
	  makeopline (chan, nick, tmp);
	  dprintf (DP_MODE, tmp);
	  mem->flags |= SENTOP;
	  nfree (tmp);
	}
      putlog (LOG_GETIN, "*", STR ("opreq from %s/%s on %s - Opped"), botnick,
	      nick, chan->dname);
    }
  else if (par[0] == 'i')
    {
      Context;
      strcpy (s, getchanmode (chan));
      p = (char *) &s;
      p2 = newsplit (&p);
      p3 = newsplit (&p);
      if (!nick[0])
	{
	  putlog (LOG_GETIN, "*",
		  STR
		  ("inreq from %s/??? for %s - No nick specified - SHOULD NOT HAPPEN"),
		  botnick, chname);
	  nfree (tmp);
	  return;
	}
      if (!chan)
	{
	  putlog (LOG_GETIN, "*",
		  STR ("inreq from %s/%s for %s - Channel %s doesn't exist"),
		  botnick, nick, chname, chname);
	  nfree (tmp);
	  return;
	}
      nfree (tmp);
      if (mem)
	{
	  putlog (LOG_GETIN, "*",
		  STR ("inreq from %s/%s for %s - %s is already on %s"),
		  botnick, nick, chan->dname, nick, chan->dname);
	  return;
	}
      if (!user)
	{
	  putlog (LOG_GETIN, "*",
		  STR
		  ("inreq from %s/%s for %s - No user called %s in userlist"),
		  botnick, nick, chan->dname, botnick);
	  return;
	}
      get_user_flagrec (user, &fr, NULL);
      if (!glob_op (fr))
	{
	  putlog (LOG_GETIN, "*",
		  STR ("inreq from %s/%s for %s - %s isn't global +o"),
		  botnick, nick, chan->dname, botnick);
	  return;
	}
      if (server_lag > lag_threshold)
	{
	  putlog (LOG_GETIN, "*",
		  STR ("inreq from %s/%s for %s - I'm too lagged"), botnick,
		  nick, chan->dname);
	  return;
	}
      if (getting_users ())
	{
	  putlog (LOG_GETIN, "*",
		  STR
		  ("inreq from %s/%s for %s - I'm getting userlist right now"),
		  botnick, nick, chan->dname);
	  return;
	}
      if (!(channel_pending (chan) || channel_active (chan)))
	{
	  putlog (LOG_GETIN, "*",
		  STR ("inreq from %s/%s for %s - I'm not on %s now"),
		  botnick, nick, chan->dname, chan->dname);
	  return;
	}
      if (strchr (p2, 'l'))
	{
	  if (!me_op (chan))
	    putlog (LOG_GETIN, "*",
		    STR ("inreq from %s/%s for %s - I haven't got ops"),
		    botnick, nick, chan->dname);
	  else
	    {
	      lim = chan->channel.members + 5;
	      if (!*p)
		curlim = atoi (p3);
	      else
		curlim = atoi (p);
	      if (curlim > 0 && curlim < lim)
		{
		  sendi = 1;
		  simple_sprintf (s2, STR ("%d"), lim);
		  add_mode (chan, '+', 'l', s2);
		  putlog (LOG_GETIN, "*",
			  STR
			  ("inreq from %s/%s for %s - Raised limit to %d"),
			  botnick, nick, chan->dname, lim, nick);
		}
	    }
	}
      mr = &global_bans;
      while (*mr)
	{
	  if (wild_match ((*mr)->mask, host))
	    {
	      if (!noshare)
		{
		  shareout (NULL, STR ("-b %s\n"), (*mr)->mask);
		}
	      putlog (LOG_GETIN, "*",
		      STR
		      ("inreq from %s/%s for %s - Removed permanent global ban %s"),
		      botnick, nick, chan->dname, (*mr)->mask);
	      nfree ((*mr)->mask);
	      if ((*mr)->desc)
		nfree ((*mr)->desc);
	      if ((*mr)->user)
		nfree ((*mr)->user);
	      tmr = *mr;
	      *mr = (*mr)->next;
	      nfree (tmr);
	    }
	  else
	    {
	      mr = &((*mr)->next);
	    }
	}
      mr = &chan->bans;
      while (*mr)
	{
	  if (wild_match ((*mr)->mask, host))
	    {
	      if (!noshare)
		{
		  shareout (NULL, STR ("-bc %s %s\n"), chan->dname,
			    (*mr)->mask);
		}
	      putlog (LOG_GETIN, "*",
		      STR
		      ("inreq from %s/%s for %s - Removed permanent channel ban %s"),
		      botnick, nick, chan->dname, (*mr)->mask);
	      nfree ((*mr)->mask);
	      if ((*mr)->desc)
		nfree ((*mr)->desc);
	      if ((*mr)->user)
		nfree ((*mr)->user);
	      tmr = *mr;
	      *mr = (*mr)->next;
	      nfree (tmr);
	    }
	  else
	    {
	      mr = &((*mr)->next);
	    }
	}
      for (b = chan->channel.ban; b->mask[0]; b = b->next)
	{
	  if (wild_match (b->mask, host))
	    {
	      add_mode (chan, '-', 'b', b->mask);
	      putlog (LOG_GETIN, "*",
		      STR ("inreq from %s/%s for %s - Removed active ban %s"),
		      botnick, nick, chan->dname, b->mask);
	      sendi = 1;
	    }
	}
      if (strchr (p2, 'k'))
	{
	  sendi = 0;
	  tmp = nmalloc (strlen (chan->dname) + strlen (p3) + 7);
	  sprintf (tmp, STR ("gi K %s %s"), chan->dname, p3);
	  botnet_send_zapf (nextbot (botnick), botnetnick, botnick, tmp);
	  putlog (LOG_GETIN, "*",
		  STR ("inreq from %s/%s for %s - Sent key (%s)"), botnick,
		  nick, chan->dname, p3);
	  nfree (tmp);
	}
      if (strchr (p2, 'i'))
	{
	  if (!me_op (chan))
	    putlog (LOG_GETIN, "*",
		    STR ("inreq from %s/%s for %s - I haven't got ops"),
		    botnick, nick, chan->dname);
	  else
	    {
	      sendi = 1;
	      putlog (LOG_GETIN, "*",
		      STR ("inreq from %s/%s for %s - Invited"), botnick,
		      nick, chan->dname);
	    }
	}
      if (sendi)
	dprintf (DP_MODE, STR ("INVITE %s %s\n"), nick, chan->dname);
    }
  else if (par[0] == 'K')
    {
      if (!chan)
	{
	  putlog (LOG_GETIN, "*",
		  STR ("Got key for nonexistant channel %s from %s"), chname,
		  botnick);
	  nfree (tmp);
	  return;
	}
      nfree (tmp);
      if (channel_inactive (chan))
	{
	  putlog (LOG_GETIN, "*",
		  STR
		  ("Got key for %s from %s - I shouldn't be on that chan?!?"),
		  chan->dname, botnick);
	}
      else
	{
	  if (!(channel_pending (chan) || channel_active (chan)))
	    {
	      putlog (LOG_GETIN, "*",
		      STR ("Got key for %s from %s (%s) - Joining"),
		      chan->dname, botnick, nick);
	      dprintf (DP_MODE, STR ("JOIN %s %s\n"), chan->dname, nick);
	    }
	  else
	    {
	      putlog (LOG_GETIN, "*",
		      STR
		      ("Got key for %s from %s - I'm already in the channel"),
		      chan->dname, botnick);
	    }
	}
    }
}
void
check_hostmask ()
{
  char s[UHOSTLEN + 2], *tmp = NULL;
  struct userrec *u = get_user_by_handle (userlist, botnetnick);
  struct list_type *q;
  if (!server_online || !botuserhost[0])
    return;
  Context;
  tmp = botuserhost;
  Context;
  if (tmp[0] != '~')
    sprintf (s, STR ("*!%s"), tmp);
  else
    {
      Context;
      tmp++;
      sprintf (s, STR ("*!*%s"), tmp);
    }
  for (q = get_user (&USERENTRY_HOSTS, u); q; q = q->next)
    {
      if (!strcasecmp (q->extra, s))
	return;
    }
  addhost_by_handle (botnetnick, s);
  putlog (LOG_GETIN, "*", STR ("Updated my hostmask: %s"), s);
}
static void
request_op (struct chanset_t *chan)
{
  int i = 0, exp = 0, first = 100, n, cnt, i2;
  memberlist *ml;
  memberlist *botops[MAX_BOTS];
  char s[100], *l, myserv[SERVLEN];
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0 };
  chan->channel.do_opreq = 0;
  if (me_op (chan))
    return;
  if (server_lag > lag_threshold)
    {
      putlog (LOG_GETIN, "*",
	      STR ("Not asking for ops on %s - I'm too lagged"), chan->dname);
      return;
    }
  n = time (NULL);
  while (i < 5)
    {
      if (n - chan->opreqtime[i] > opreq_seconds)
	{
	  if (first > i)
	    first = i;
	  exp++;
	  chan->opreqtime[i] = 0;
	}
      i++;
    }
  if ((5 - exp) >= opreq_count)
    {
      putlog (LOG_GETIN, "*",
	      STR ("Delaying opreq for %s - Maximum of %d:%d reached"),
	      chan->dname, opreq_count, opreq_seconds);
      return;
    }
  check_hostmask ();
  i = 0;
  ml = chan->channel.member;
  myserv[0] = 0;
  while ((i < MAX_BOTS) && (ml) && ml->nick[0])
    {
      if ((i < MAX_BOTS) && (ml->user))
	{
	  get_user_flagrec (ml->user, &fr, NULL);
	  if (bot_hublevel (ml->user) == 999 && glob_bot (fr) && glob_op (fr)
	      && !glob_deop (fr) && chan_hasop (ml) && !chan_issplit (ml)
	      && nextbot (ml->user->handle) >= 0)
	    botops[i++] = ml;
	}
      if (!strcmp (ml->nick, botname))
	if (ml->server)
	  strcpy (myserv, ml->server);
      ml = ml->next;
    }
  if (!i)
    {
      putlog (LOG_GETIN, "@", STR ("Noone to ask for ops on %s"),
	      chan->dname);
      return;
    }
  cnt = op_bots;
  sprintf (s, STR ("gi o %s %s"), chan->dname, botname);
  l = nmalloc (cnt * 50);
  l[0] = 0;
  for (i2 = 0; i2 < i; i2++)
    {
      if (botops[i2]->server && (!strcmp (botops[i2]->server, myserv)))
	{
	  botnet_send_zapf (nextbot (botops[i2]->user->handle), botnetnick,
			    botops[i2]->user->handle, s);
	  chan->opreqtime[first] = n;
	  if (l[0])
	    {
	      strcat (l, ", ");
	      strcat (l, botops[i2]->user->handle);
	    }
	  else
	    {
	      strcpy (l, botops[i2]->user->handle);
	    }
	  strcat (l, "/");
	  strcat (l, botops[i2]->nick);
	  botops[i2] = NULL;
	  cnt--;
	  break;
	}
    }
  while (cnt)
    {
      i2 = random () % i;
      if (botops[i2])
	{
	  botnet_send_zapf (nextbot (botops[i2]->user->handle), botnetnick,
			    botops[i2]->user->handle, s);
	  chan->opreqtime[first] = n;
	  if (l[0])
	    {
	      strcat (l, ", ");
	      strcat (l, botops[i2]->user->handle);
	    }
	  else
	    {
	      strcpy (l, botops[i2]->user->handle);
	    }
	  strcat (l, "/");
	  strcat (l, botops[i2]->nick);
	  cnt--;
	  botops[i2] = NULL;
	}
      else
	{
	  if (i < op_bots)
	    cnt--;
	}
    }
  putlog (LOG_GETIN, "*", STR ("Requested ops on %s from %s"), chan->dname,
	  l);
  nfree (l);
}
static void
request_in (struct chanset_t *chan)
{
  char s[255], *l;
  int i = 0;
  int cnt, n;
  struct userrec *botops[MAX_BOTS];
  struct userrec *user;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0 };
  for (user = userlist; user && (i < MAX_BOTS); user = user->next)
    {
      get_user_flagrec (user, &fr, NULL);
      if (bot_hublevel (user) == 999 && glob_bot (fr) && glob_op (fr)
#ifdef G_BACKUP
	  && (!glob_backupbot (fr) || channel_backup (chan))
#endif
	  && nextbot (user->handle) >= 0)
	botops[i++] = user;
    }
  if (!i)
    {
      putlog (LOG_GETIN, "*",
	      STR ("No bots linked, can't request help to join %s"),
	      chan->dname);
      return;
    }
  check_hostmask ();
  cnt = in_bots;
  sprintf (s, STR ("gi i %s %s %s!%s"), chan->dname, botname, botname,
	   botuserhost);
  l = nmalloc (cnt * 30);
  l[0] = 0;
  while (cnt)
    {
      n = random () % i;
      if (botops[n])
	{
	  botnet_send_zapf (nextbot (botops[n]->handle), botnetnick,
			    botops[n]->handle, s);
	  if (l[0])
	    {
	      strcat (l, ", ");
	      strcat (l, botops[n]->handle);
	    }
	  else
	    {
	      strcpy (l, botops[n]->handle);
	    }
	  botops[n] = NULL;
	  cnt--;
	}
      else
	{
	  if (i < in_bots)
	    cnt--;
	}
    }
  putlog (LOG_GETIN, "*", STR ("Requesting help to join %s from %s"),
	  chan->dname, l);
  nfree (l);
}
static int
want_to_revenge (struct chanset_t *chan, struct userrec *u,
		 struct userrec *u2, char *badnick, char *victim,
		 int mevictim)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  if (match_my_nick (badnick))
    return 0;
  get_user_flagrec (u, &fr, chan->dname);
  if (!chan_friend (fr) && !glob_friend (fr) && rfc_casecmp (badnick, victim))
    {
      if (mevictim)
	{
	  if (channel_revengebot (chan))
	    return 1;
	}
      else if (channel_revenge (chan) && u2)
	{
	  struct flag_record fr2 = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
	  get_user_flagrec (u2, &fr2, chan->dname);
	  if ((channel_protectfriends (chan)
	       && (chan_friend (fr2)
		   || (glob_friend (fr2) && !chan_deop (fr2))))
	      || (channel_protectops (chan)
		  && (chan_op (fr2) || (glob_op (fr2) && !chan_deop (fr2)))))
	    return 1;
	}
    }
  return 0;
}
static void
punish_badguy (struct chanset_t *chan, char *whobad, struct userrec *u,
	       char *badnick, char *victim, int mevictim, int type)
{
  char reason[1024], ct[81], *kick_msg;
  memberlist *m;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  m = ismember (chan, badnick);
  if (!m)
    return;
  get_user_flagrec (u, &fr, chan->dname);
  egg_strftime (ct, 7, "%d %b", localtime (&now));
  reason[0] = 0;
  switch (type)
    {
    case REVENGE_KICK:
      kick_msg = IRC_KICK_PROTECT;
      simple_sprintf (reason, "kicked %s off %s", victim, chan->dname);
      break;
    case REVENGE_DEOP:
      simple_sprintf (reason, "deopped %s on %s", victim, chan->dname);
      kick_msg = IRC_DEOP_PROTECT;
      break;
    default:
      kick_msg = "revenge!";
    }
  putlog (LOG_MISC, chan->dname, "Punishing %s (%s)", badnick, reason);
  if ((chan->revenge_mode > 0) && !(chan_deop (fr) || glob_deop (fr)))
    {
      char s[UHOSTLEN], s1[UHOSTLEN];
      memberlist *mx = NULL;
      if (chan_op (fr) || (glob_op (fr) && !chan_deop (fr)))
	{
	  fr.match = FR_CHAN;
	  if (chan_op (fr))
	    {
	      fr.chan &= ~USER_OP;
	    }
	  else
	    {
	      fr.chan |= USER_DEOP;
	    }
	  set_user_flagrec (u, &fr, chan->dname);
	  putlog (LOG_MISC, "*", "No longer opping %s[%s] (%s)", u->handle,
		  whobad, reason);
	}
      else if (u)
	{
	  fr.match = FR_CHAN;
	  fr.chan |= USER_DEOP;
	  set_user_flagrec (u, &fr, chan->dname);
	  simple_sprintf (s, "(%s) %s", ct, reason);
	  putlog (LOG_MISC, "*", "Now deopping %s[%s] (%s)", u->handle,
		  whobad, s);
	}
      else
	{
	  strcpy (s1, whobad);
	  maskhost (s1, s);
	  strcpy (s1, badnick);
	  while (get_user_by_handle (userlist, s1))
	    {
	      if (!strncmp (s1, "bad", 3))
		{
		  int i;
		  i = atoi (s1 + 3);
		  simple_sprintf (s1 + 3, "%d", i + 1);
		}
	      else
		strcpy (s1, "bad1");
	    }
	  userlist = adduser (userlist, s1, s, "-", 0);
	  fr.match = FR_CHAN;
	  fr.chan = USER_DEOP;
	  fr.udef_chan = 0;
	  u = get_user_by_handle (userlist, s1);
	  if ((mx = ismember (chan, badnick)))
	    mx->user = u;
	  set_user_flagrec (u, &fr, chan->dname);
	  simple_sprintf (s, "(%s) %s (%s)", ct, reason, whobad);
	  set_user (&USERENTRY_COMMENT, u, (void *) s);
	  putlog (LOG_MISC, "*", "Now deopping %s (%s)", whobad, reason);
    }}
  if (!mevictim)
    add_mode (chan, '-', 'o', badnick);
  if (chan->revenge_mode > 2)
    {
      char s[UHOSTLEN], s1[UHOSTLEN];
      splitnick (&whobad);
      maskhost (whobad, s1);
      simple_sprintf (s, "(%s) %s", ct, reason);
      u_addban (chan, s1, botnetnick, s, now + (60 * chan->ban_time), 0);
      if (!mevictim && me_op (chan))
	{
	  add_mode (chan, '+', 'b', s1);
	  flush_mode (chan, QUICK);
	}
    }
  if ((chan->revenge_mode > 1)
      && (!channel_dontkickops (chan)
	  || !(chan_op (fr) || (glob_op (fr) && !chan_deop (fr))))
      && !chan_sentkick (m) && me_op (chan) && !mevictim)
    {
      dprintf (DP_MODE, "KICK %s %s :%s\n", chan->name, badnick, kick_msg);
      m->flags |= SENTKICK;
    }
}
static void
maybe_revenge (struct chanset_t *chan, char *whobad, char *whovictim,
	       int type)
{
  char *badnick, *victim;
  int mevictim;
  struct userrec *u, *u2;
  if (!chan || (type < 0))
    return;
  u = get_user_by_host (whobad);
  badnick = splitnick (&whobad);
  u2 = get_user_by_host (whovictim);
  victim = splitnick (&whovictim);
  mevictim = match_my_nick (victim);
  if (!want_to_revenge (chan, u, u2, badnick, victim, mevictim))
    return;
  punish_badguy (chan, whobad, u, badnick, victim, mevictim, type);
}
static void
set_key (struct chanset_t *chan, char *k)
{
  nfree (chan->channel.key);
  if (k == NULL)
    {
      chan->channel.key = (char *) channel_malloc (1);
      chan->channel.key[0] = 0;
      return;
    }
  chan->channel.key = (char *) channel_malloc (strlen (k) + 1);
  strcpy (chan->channel.key, k);
} static void
newmask (masklist * m, char *s, char *who)
{
  for (; m && m->mask[0] && rfc_casecmp (m->mask, s); m = m->next);
  if (m->mask[0])
    return;
  m->next = (masklist *) channel_malloc (sizeof (masklist));
  m->next->next = NULL;
  m->next->mask = (char *) channel_malloc (1);
  m->next->mask[0] = 0;
  nfree (m->mask);
  m->mask = (char *) channel_malloc (strlen (s) + 1);
  strcpy (m->mask, s);
  m->who = (char *) channel_malloc (strlen (who) + 1);
  strcpy (m->who, who);
  m->timer = now;
} static int
killmember (struct chanset_t *chan, char *nick)
{
  memberlist *x, *old;
  old = NULL;
  for (x = chan->channel.member; x && x->nick[0]; old = x, x = x->next)
    if (!rfc_casecmp (x->nick, nick))
      break;
  if (!x || !x->nick[0])
    {
      if (!channel_pending (chan))
	putlog (LOG_MISC, "*", "(!) killmember(%s) -> nonexistent", nick);
      return 0;
    }
  if (old)
    old->next = x->next;
  else
    chan->channel.member = x->next;
  nfree (x);
  chan->channel.members--;
  if (chan->channel.members < 0)
    {
      chan->channel.members = 0;
      for (x = chan->channel.member; x && x->nick[0]; x = x->next)
	chan->channel.members++;
      putlog (LOG_MISC, "*", "(!) actually I know of %d members.",
	      chan->channel.members);
    }
  if (!chan->channel.member)
    {
      chan->channel.member =
	(memberlist *) channel_malloc (sizeof (memberlist));
      chan->channel.member->nick[0] = 0;
      chan->channel.member->next = NULL;
    }
  return 1;
}
static int
me_op (struct chanset_t *chan)
{
  memberlist *mx = NULL;
  mx = ismember (chan, botname);
  if (!mx)
    return 0;
  if (chan_hasop (mx))
    return 1;
  else
    return 0;
}
static int
me_voice (struct chanset_t *chan)
{
  memberlist *mx;
  mx = ismember (chan, botname);
  if (!mx)
    return 0;
  if (chan_hasvoice (mx))
    return 1;
  else
    return 0;
}
static int
any_ops (struct chanset_t *chan)
{
  memberlist *x;
  for (x = chan->channel.member; x && x->nick[0]; x = x->next)
    if (chan_hasop (x))
      break;
  if (!x || !x->nick[0])
    return 0;
  return 1;
}
static void
reset_chan_info (struct chanset_t *chan)
{
  if (channel_inactive (chan))
    {
      dprintf (DP_MODE, "PART %s\n", chan->name);
      return;
    }
  if (!channel_pending (chan))
    {
      nfree (chan->channel.key);
      chan->channel.key = (char *) channel_malloc (1);
      chan->channel.key[0] = 0;
      clear_channel (chan, 1);
      chan->status |= CHAN_PEND;
      chan->status &= ~(CHAN_ACTIVE | CHAN_ASKEDMODES);
      if (!(chan->status & CHAN_ASKEDBANS))
	{
	  chan->status |= CHAN_ASKEDBANS;
	  dprintf (DP_MODE, "MODE %s +b\n", chan->name);
	}
#ifdef S_IRCNET
      if (!(chan->ircnet_status & CHAN_ASKED_EXEMPTS) && use_exempts == 1)
	{
	  chan->ircnet_status |= CHAN_ASKED_EXEMPTS;
	  dprintf (DP_MODE, "MODE %s +e\n", chan->name);
	}
      if (!(chan->ircnet_status & CHAN_ASKED_INVITED) && use_invites == 1)
	{
	  chan->ircnet_status |= CHAN_ASKED_INVITED;
	  dprintf (DP_MODE, "MODE %s +I\n", chan->name);
	}
#endif
      dprintf (DP_MODE, "MODE %s\n", chan->name);
      if (use_354)
	dprintf (DP_MODE, "WHO %s %%c%%h%%n%%u%%f\n", chan->name);
      else
	dprintf (DP_MODE, "WHO %s\n", chan->name);
    }
}
static void
do_channel_part (struct chanset_t *chan)
{
  if (!channel_inactive (chan) && chan->name[0])
    {
      dprintf (DP_SERVER, "PART %s\n", chan->name);
      check_tcl_part (botname, botuserhost, NULL, chan->dname, NULL);
    }
}
static void
check_lonely_channel (struct chanset_t *chan)
{
  memberlist *m;
  char s[UHOSTLEN];
  int i = 0;
  static int whined = 0;
  if (channel_pending (chan) || !channel_active (chan) || me_op (chan)
      || channel_inactive (chan) || (chan->channel.mode & CHANANON))
    return;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
    if (!chan_issplit (m))
      i++;
  if (i == 1 && channel_cycle (chan) && !channel_stop_cycle (chan))
    {
      if (chan->name[0] != '+')
	{
	  putlog (LOG_MISC, "*", "Trying to cycle %s to regain ops.",
		  chan->dname);
	  dprintf (DP_MODE, "PART %s\n", chan->name);
	  dprintf (DP_MODE, "JOIN %s%s %s\n",
		   (chan->dname[0] == '!') ? "!" : "", chan->dname,
		   chan->key_prot);
	  whined = 0;
	}
    }
  else if (any_ops (chan))
    {
      whined = 0;
      request_op (chan);
    }
  else
    {
      int ok = 1;
      struct userrec *u;
      if (!whined)
	{
	  if (chan->name[0] != '+')
	    putlog (LOG_MISC, "*", "%s is active but has no ops :(",
		    chan->dname);
	  whined = 1;
	}
      for (m = chan->channel.member; m && m->nick[0]; m = m->next)
	{
	  sprintf (s, "%s!%s", m->nick, m->userhost);
	  u = get_user_by_host (s);
	  if (!match_my_nick (m->nick) && (!u || !(u->flags & USER_BOT)))
	    {
	      ok = 0;
	      break;
	    }
	}
      if (ok && channel_cycle (chan))
	{
	}
      else
	{
	  request_op (chan);
	}
    }
}
void
check_servers ()
{
  struct chanset_t *chan;
  memberlist *m;
  for (chan = chanset; chan; chan = chan->next)
    {
      for (m = chan->channel.member; m && m->nick[0]; m = m->next)
	if (chan_hasop (m) && (!m->server || !m->server[0]))
	  {
	    dprintf (DP_HELP, STR ("WHO %s\n"), chan->name);
	    return;
	  }
    }
}
void
raise_limit (struct chanset_t *chan)
{
  int nl, cl, i, mem, ul, ll;
  char s[50];
  if (!chan)
    return;
  if (!me_op (chan))
    return;
  mem = chan->channel.members;
  nl = mem + chan->limitraise;
  cl = chan->channel.maxmembers;
  i = chan->limitraise / 4;
  ul = nl + i;
  ll = nl - i;
  if (cl > ll && cl < ul)
    return;
  if (nl != chan->channel.maxmembers)
    {
      sprintf (s, "%d", nl);
      add_mode (chan, '+', 'l', s);
    }
}
static void
check_expired_chanstuff ()
{
  masklist *b, *e;
  memberlist *m, *n;
  char s[UHOSTLEN];
  struct chanset_t *chan;
  struct userrec *buser;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  struct flag_record fr3 = { FR_GLOBAL | FR_CHAN, 0, 0 };
  if (!server_online)
    return;
  for (chan = chanset; chan; chan = chan->next)
    {
      if (channel_active (chan))
	{
	  if (me_op (chan))
	    {
	      if (channel_dynamicbans (chan) && chan->ban_time)
		for (b = chan->channel.ban; b->mask[0]; b = b->next)
		  if (now - b->timer > 60 * chan->ban_time
		      && !u_sticky_mask (chan->bans, b->mask)
		      && !u_sticky_mask (global_bans, b->mask)
		      && expired_mask (chan, b->who))
		    {
		      putlog (LOG_MODES, chan->dname,
			      "(%s) Channel ban on %s expired.", chan->dname,
			      b->mask);
		      add_mode (chan, '-', 'b', b->mask);
		      b->timer = now;
		    }
#ifdef S_IRCNET
	      if (use_exempts && channel_dynamicexempts (chan)
		  && chan->exempt_time)
		for (e = chan->channel.exempt; e->mask[0]; e = e->next)
		  if (now - e->timer > 60 * chan->exempt_time
		      && !u_sticky_mask (chan->exempts, e->mask)
		      && !u_sticky_mask (global_exempts, e->mask)
		      && expired_mask (chan, e->who))
		    {
		      int match = 0;
		      for (b = chan->channel.ban; b->mask[0]; b = b->next)
			if (wild_match (b->mask, e->mask)
			    || wild_match (e->mask, b->mask))
			  {
			    match = 1;
			    break;
			  }
		      if (match)
			{
			  putlog (LOG_MODES, chan->dname,
				  "(%s) Channel exemption %s NOT expired. Exempt still set!",
				  chan->dname, e->mask);
			}
		      else
			{
			  putlog (LOG_MODES, chan->dname,
				  "(%s) Channel exemption on %s expired.",
				  chan->dname, e->mask);
			  add_mode (chan, '-', 'e', e->mask);
			}
		      e->timer = now;
		    }
	      if (use_invites && channel_dynamicinvites (chan)
		  && chan->invite_time && !(chan->channel.mode & CHANINV))
		for (b = chan->channel.invite; b->mask[0]; b = b->next)
		  if (now - b->timer > 60 * chan->invite_time
		      && !u_sticky_mask (chan->invites, b->mask)
		      && !u_sticky_mask (global_invites, b->mask)
		      && expired_mask (chan, b->who))
		    {
		      putlog (LOG_MODES, chan->dname,
			      "(%s) Channel invitation on %s expired.",
			      chan->dname, b->mask);
		      add_mode (chan, '-', 'I', b->mask);
		      b->timer = now;
		    }
#endif
	      if (chan->idle_kick)
		for (m = chan->channel.member; m && m->nick[0]; m = m->next)
		  if (now - m->last >= chan->idle_kick * 60
		      && !match_my_nick (m->nick) && !chan_issplit (m))
		    {
		      sprintf (s, "%s!%s", m->nick, m->userhost);
		      get_user_flagrec (m->user ? m->
					user : get_user_by_host (s), &fr,
					chan->dname);
		      if (!
			  (glob_bot (fr) || glob_friend (fr)
			   || (glob_op (fr) && !glob_deop (fr))
			   || chan_friend (fr) || chan_op (fr)))
			{
			  dprintf (DP_SERVER, "KICK %s %s :idle %d min\n",
				   chan->name, m->nick, chan->idle_kick);
			  m->flags |= SENTKICK;
			}
		    }
	    }
	  for (m = chan->channel.member; m && m->nick[0]; m = n)
	    {
	      n = m->next;
	      if (m->split && now - m->split > wait_split)
		{
		  sprintf (s, "%s!%s", m->nick, m->userhost);
		  check_tcl_sign (m->nick, m->userhost,
				  m->user ? m->user : get_user_by_host (s),
				  chan->dname, "lost in the netsplit");
		  putlog (LOG_JOIN, chan->dname,
			  "%s (%s) got lost in the net-split.", m->nick,
			  m->userhost);
		  killmember (chan, m->nick);
		}
	      m = n;
	    }
	  buser = get_user_by_handle (userlist, botnetnick);
	  get_user_flagrec (buser, &fr3, chan->name);
	  if (!loading && channel_active (chan) && me_op (chan) && (buser)
	      && (chan_dovoice (fr3) || glob_dovoice (fr3)))
	    {
	      for (m = chan->channel.member; m && m->nick[0]; m = m->next)
		{
		  if (m->user)
		    {
		      struct flag_record fr2 = { FR_GLOBAL | FR_CHAN, 0, 0 };
		      get_user_flagrec (m->user, &fr2, chan->name);
		      if ((!glob_bot (fr2)
			   &&
			   ((chan_voice (fr2)
			     || (glob_voice (fr2) && !chan_quiet (fr2)))
			    || (channel_voice (chan)
				&& (!chan_quiet (fr2) && !glob_quiet (fr2))))
			   && !chan_hasop (m) && !chan_hasvoice (m)
			   && !(m->flags & EVOICE)))
			{
			  add_mode (chan, '+', 'v', m->nick);
			}
		      else if (!glob_bot (fr2)
			       &&
			       ((chan_quiet (fr2)
				 || (glob_quiet (fr2) && !chan_voice (fr2)))
				|| (m->flags & EVOICE)))
			{
			  if (!chan_hasop (m) && chan_hasvoice (m))
			    add_mode (chan, '-', 'v', m->nick);
			}
		    }
		  else if (m->user == NULL && !(m->flags & EVOICE))
		    if (channel_voice (chan) && !chan_hasop (m)
			&& !chan_hasvoice (m))
		      add_mode (chan, '+', 'v', m->nick);
		}
	    }
	  check_lonely_channel (chan);
	}
      else if (!channel_inactive (chan) && !channel_pending (chan))
	dprintf (DP_MODE, "JOIN %s %s\n",
		 (chan->name[0]) ? chan->name : chan->dname,
		 chan->channel.key[0] ? chan->channel.key : chan->key_prot);
    }
}
static int channels_6char STDVAR
{
  Function F = (Function) cd;
  char x[20];
    BADARGS (7, 7, " nick user@host handle desto/chan keyword/nick text");
    CHECKVALIDITY (channels_6char);
    sprintf (x, "%d",
	     F (argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]));
    Tcl_AppendResult (irp, x, NULL);
    return TCL_OK;
}
static int channels_5char STDVAR
{
  Function F = (Function) cd;
    BADARGS (6, 6, " nick user@host handle channel text");
    CHECKVALIDITY (channels_5char);
    F (argv[1], argv[2], argv[3], argv[4], argv[5]);
    return TCL_OK;
}
static int channels_4char STDVAR
{
  Function F = (Function) cd;
    BADARGS (5, 5, " nick uhost hand chan/param");
    CHECKVALIDITY (channels_4char);
    F (argv[1], argv[2], argv[3], argv[4]);
    return TCL_OK;
}
static int channels_2char STDVAR
{
  Function F = (Function) cd;
    BADARGS (3, 3, " channel type");
    CHECKVALIDITY (channels_2char);
    F (argv[1], argv[2]);
    return TCL_OK;
}
static void
check_tcl_joinspltrejn (char *nick, char *uhost, struct userrec *u,
			char *chname, p_tcl_bind_list table)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  char args[1024];
  simple_sprintf (args, "%s %s!%s", chname, nick, uhost);
  get_user_flagrec (u, &fr, chname);
  Tcl_SetVar (interp, "_jp1", nick, 0);
  Tcl_SetVar (interp, "_jp2", uhost, 0);
  Tcl_SetVar (interp, "_jp3", u ? u->handle : "*", 0);
  Tcl_SetVar (interp, "_jp4", chname, 0);
  check_tcl_bind (table, args, &fr, " $_jp1 $_jp2 $_jp3 $_jp4",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
} static void
check_tcl_part (char *nick, char *uhost, struct userrec *u, char *chname,
		char *text)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  char args[1024];
  simple_sprintf (args, "%s %s!%s", chname, nick, uhost);
  get_user_flagrec (u, &fr, chname);
  Tcl_SetVar (interp, "_p1", nick, 0);
  Tcl_SetVar (interp, "_p2", uhost, 0);
  Tcl_SetVar (interp, "_p3", u ? u->handle : "*", 0);
  Tcl_SetVar (interp, "_p4", chname, 0);
  Tcl_SetVar (interp, "_p5", text ? text : "", 0);
  check_tcl_bind (H_part, args, &fr, " $_p1 $_p2 $_p3 $_p4 $_p5",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
} static void
check_tcl_signtopcnick (char *nick, char *uhost, struct userrec *u,
			char *chname, char *reason, p_tcl_bind_list table)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  char args[1024];
  if (table == H_sign)
    simple_sprintf (args, "%s %s!%s", chname, nick, uhost);
  else
    simple_sprintf (args, "%s %s", chname, reason);
  get_user_flagrec (u, &fr, chname);
  Tcl_SetVar (interp, "_stnm1", nick, 0);
  Tcl_SetVar (interp, "_stnm2", uhost, 0);
  Tcl_SetVar (interp, "_stnm3", u ? u->handle : "*", 0);
  Tcl_SetVar (interp, "_stnm4", chname, 0);
  Tcl_SetVar (interp, "_stnm5", reason, 0);
  check_tcl_bind (table, args, &fr,
		  " $_stnm1 $_stnm2 $_stnm3 $_stnm4 $_stnm5",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
}
static void
check_tcl_kickmode (char *nick, char *uhost, struct userrec *u, char *chname,
		    char *dest, char *reason, p_tcl_bind_list table)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  char args[512];
  get_user_flagrec (u, &fr, chname);
  if (table == H_mode)
    simple_sprintf (args, "%s %s", chname, dest);
  else
    simple_sprintf (args, "%s %s %s", chname, dest, reason);
  Tcl_SetVar (interp, "_kick1", nick, 0);
  Tcl_SetVar (interp, "_kick2", uhost, 0);
  Tcl_SetVar (interp, "_kick3", u ? u->handle : "*", 0);
  Tcl_SetVar (interp, "_kick4", chname, 0);
  Tcl_SetVar (interp, "_kick5", dest, 0);
  Tcl_SetVar (interp, "_kick6", reason, 0);
  check_tcl_bind (table, args, &fr,
		  " $_kick1 $_kick2 $_kick3 $_kick4 $_kick5 $_kick6",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
}
static int
check_tcl_pub (char *nick, char *from, char *chname, char *msg)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  int x;
  char buf[512], *args = buf, *cmd, host[161], *hand;
  struct userrec *u;
  strcpy (args, msg);
  cmd = newsplit (&args);
  simple_sprintf (host, "%s!%s", nick, from);
  u = get_user_by_host (host);
  hand = u ? u->handle : "*";
  get_user_flagrec (u, &fr, chname);
  Tcl_SetVar (interp, "_pub1", nick, 0);
  Tcl_SetVar (interp, "_pub2", from, 0);
  Tcl_SetVar (interp, "_pub3", hand, 0);
  Tcl_SetVar (interp, "_pub4", chname, 0);
  Tcl_SetVar (interp, "_pub5", args, 0);
  x =
    check_tcl_bind (H_pub, cmd, &fr, " $_pub1 $_pub2 $_pub3 $_pub4 $_pub5",
		    MATCH_EXACT | BIND_USE_ATTR | BIND_HAS_BUILTINS);
  if (x == BIND_NOMATCH)
    return 0;
  if (x == BIND_EXEC_LOG)
    putlog (LOG_CMDS, chname, "<<%s>> !%s! %s %s", nick, hand, cmd, args);
  return 1;
}
static void
check_tcl_pubm (char *nick, char *from, char *chname, char *msg)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  char buf[1024], host[161];
  struct userrec *u;
  simple_sprintf (buf, "%s %s", chname, msg);
  simple_sprintf (host, "%s!%s", nick, from);
  u = get_user_by_host (host);
  get_user_flagrec (u, &fr, chname);
  Tcl_SetVar (interp, "_pubm1", nick, 0);
  Tcl_SetVar (interp, "_pubm2", from, 0);
  Tcl_SetVar (interp, "_pubm3", u ? u->handle : "*", 0);
  Tcl_SetVar (interp, "_pubm4", chname, 0);
  Tcl_SetVar (interp, "_pubm5", msg, 0);
  check_tcl_bind (H_pubm, buf, &fr,
		  " $_pubm1 $_pubm2 $_pubm3 $_pubm4 $_pubm5",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
} static tcl_ints myints[] =
  { {"learn-users", &learn_users, 0}, {"wait-split", &wait_split, 0},
  {"wait-info", &wait_info, 0}, {"bounce-bans", &bounce_bans, 0},
  {"bounce-exempts", &bounce_exempts, 0}, {"bounce-invites", &bounce_invites,
					   0}, {"bounce-modes", &bounce_modes,
						0}, {"modes-per-line",
						     &modesperline, 0},
  {"mode-buf-length", &mode_buf_len, 0}, {"use-354", &use_354, 0},
  {"kick-method", &kick_method, 0}, {"kick-fun", &kick_fun, 0}, {"ban-fun",
								 &ban_fun, 0},
  {"invite-key", &invite_key, 0}, {"no-chanrec-info", &no_chanrec_info, 0},
  {"max-bans", &max_bans, 0}, {"max-exempts", &max_exempts, 0},
  {"max-invites", &max_invites, 0}, {"max-modes", &max_modes, 0}, {"net-type",
								   &net_type,
								   0},
  {"strict-host", &strict_host, 0}, {"ctcp-mode", &ctcp_mode, 0},
  {"keep-nick", &keepnick, 0}, {"prevent-mixing", &prevent_mixing, 0},
  {"rfc-compliant", &rfc_compliant, 0}, {"include-lk", &include_lk, 0}, {NULL,
									 NULL,
									 0} };
static void
flush_modes ()
{
  struct chanset_t *chan;
  memberlist *m;
  if (modesperline > MODES_PER_LINE_MAX)
    modesperline = MODES_PER_LINE_MAX;
  for (chan = chanset; chan; chan = chan->next)
    {
      for (m = chan->channel.member; m && m->nick[0]; m = m->next)
	{
	  if (m->delay && m->delay <= now)
	    {
	      m->delay = 0L;
	      m->flags &= ~FULL_DELAY;
	      if (chan_sentop (m))
		{
		  m->flags &= ~SENTOP;
		  add_mode (chan, '+', 'o', m->nick);
		}
	      if (chan_sentvoice (m))
		{
		  m->flags &= ~SENTVOICE;
		  add_mode (chan, '+', 'v', m->nick);
		}
	    }
	}
      flush_mode (chan, NORMAL);
    }
}
static void
irc_report (int idx, int details)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  char ch[1024], q[160], *p;
  int k, l;
  struct chanset_t *chan;
  strcpy (q, "Channels: ");
  k = 10;
  for (chan = chanset; chan; chan = chan->next)
    {
      if (idx != DP_STDOUT)
	get_user_flagrec (dcc[idx].user, &fr, chan->dname);
      if (idx == DP_STDOUT || glob_master (fr) || chan_master (fr))
	{
	  p = NULL;
	  if (!channel_inactive (chan))
	    {
	      if (chan->status & CHAN_JUPED)
		p = MISC_JUPED;
	      else if (!(chan->status & CHAN_ACTIVE))
		p = MISC_TRYING;
	      else if (chan->status & CHAN_PEND)
		p = MISC_PENDING;
	      else if ((chan->dname[0] != '+') && !me_op (chan))
		p = MISC_WANTOPS;
	    }
	  l =
	    simple_sprintf (ch, "%s%s%s%s, ", chan->dname, p ? "(" : "",
			    p ? p : "", p ? ")" : "");
	  if ((k + l) > 70)
	    {
	      dprintf (idx, "   %s\n", q);
	      strcpy (q, "          ");
	      k = 10;
	    }
	  k += my_strcpy (q + k, ch);
	}
    }
  if (k > 10)
    {
      q[k - 2] = 0;
      dprintf (idx, "    %s\n", q);
    }
}
static void
do_nettype ()
{
  switch (net_type)
    {
    case 0:
      kick_method = 1;
      modesperline = 4;
      use_354 = 0;
#ifdef S_IRCNET
      use_exempts = 0;
      use_invites = 0;
#endif
      max_bans = 25;
      max_modes = 20;
      rfc_compliant = 1;
      include_lk = 0;
      break;
    case 1:
      kick_method = 4;
      modesperline = 3;
      use_354 = 0;
#ifdef S_IRCNET
      use_exempts = 1;
      use_invites = 1;
#endif
      max_bans = 30;
      max_modes = 30;
      rfc_compliant = 1;
      include_lk = 1;
      break;
    case 2:
      kick_method = 1;
      modesperline = 6;
      use_354 = 1;
#ifdef S_IRCNET
      use_exempts = 0;
      use_invites = 0;
#endif
      max_bans = 30;
      max_modes = 30;
      rfc_compliant = 1;
      include_lk = 1;
      break;
    case 3:
      kick_method = 1;
      modesperline = 6;
      use_354 = 0;
#ifdef S_IRCNET
      use_exempts = 0;
      use_invites = 0;
#endif
      max_bans = 100;
      max_modes = 100;
      rfc_compliant = 0;
      include_lk = 1;
      break;
    case 4:
      kick_method = 1;
      modesperline = 4;
      use_354 = 0;
#ifdef S_IRCNET
      use_exempts = 1;
      use_invites = 0;
#endif
      max_bans = 20;
      max_modes = 20;
      rfc_compliant = 1;
      include_lk = 0;
      break;
    default:
      break;
    }
  add_hook (HOOK_RFC_CASECMP, (Function) rfc_compliant);
}

#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
static char *
traced_nettype (ClientData cdata, Tcl_Interp * irp, CONST char *name1,
		CONST char *name2, int flags)
#else
static char *
traced_nettype (ClientData cdata, Tcl_Interp * irp, char *name1, char *name2,
		int flags)
#endif
{
  do_nettype ();
  return NULL;
}

#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
static char *
traced_rfccompliant (ClientData cdata, Tcl_Interp * irp, CONST char *name1,
		     CONST char *name2, int flags)
#else
static char *
traced_rfccompliant (ClientData cdata, Tcl_Interp * irp, char *name1,
		     char *name2, int flags)
#endif
{
  add_hook (HOOK_RFC_CASECMP, (Function) rfc_compliant);
  return NULL;
}
static int
irc_expmem ()
{
  return 0;
}
static cmd_t irc_bot[] =
  { {"dp", "", (Function) mdop_request, NULL}, {"gi", "",
						(Function) getin_request,
						NULL}, {0, 0, 0, 0} };
static void
channel_check_locked (struct chanset_t *chan)
{
  if (!chan)
    return;
  if (!me_op (chan))
    return;
  if (!strchr (getchanmode (chan), 'i'))
    dprintf (DP_MODE, STR ("MODE %s +i\n"), chan->name);
  priority_do (chan, 0, PRIO_KICK);
}
static void
getin_3secondly ()
{
  struct chanset_t *ch = chanset;
  if (!server_online)
    return;
  while (ch)
    {
      if ((channel_pending (ch) || channel_active (ch)) && (!me_op (ch)))
	request_op (ch);
      ch = ch->next;
    }
}
static void
irc_10secondly ()
{
  struct chanset_t *ch = chanset;
  for (ch = chanset; ch; ch = ch->next)
    if (channel_closed (ch))
      channel_check_locked (ch);
}
static char *
irc_close ()
{
  struct chanset_t *chan;
  dprintf (DP_MODE, "JOIN 0\n");
  for (chan = chanset; chan; chan = chan->next)
    clear_channel (chan, 1);
  del_bind_table (H_topc);
  del_bind_table (H_splt);
  del_bind_table (H_sign);
  del_bind_table (H_rejn);
  del_bind_table (H_part);
  del_bind_table (H_nick);
  del_bind_table (H_mode);
  del_bind_table (H_kick);
  del_bind_table (H_join);
  del_bind_table (H_pubm);
  del_bind_table (H_pub);
  del_bind_table (H_need);
  rem_tcl_ints (myints);
  rem_builtins (H_bot, irc_bot);
  rem_builtins_dcc (H_dcc, irc_dcc);
  rem_builtins (H_msg, C_msg);
  rem_builtins (H_raw, irc_raw);
  rem_tcl_commands (tclchan_cmds);
  del_hook (HOOK_MINUTELY, (Function) check_expired_chanstuff);
  del_hook (HOOK_MINUTELY, (Function) check_servers);
  del_hook (HOOK_ADD_MODE, (Function) real_add_mode);
  del_hook (HOOK_IDLE, (Function) flush_modes);
  del_hook (HOOK_3SECONDLY, (Function) getin_3secondly);
  del_hook (HOOK_10SECONDLY, (Function) irc_10secondly);
  Tcl_UntraceVar (interp, "rfc-compliant",
		  TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		  traced_rfccompliant, NULL);
  Tcl_UntraceVar (interp, "net-type",
		  TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		  traced_nettype, NULL);
  module_undepend (MODULE_NAME);
  return NULL;
}
EXPORT_SCOPE char *irc_start ();
static Function irc_table[] =
  { (Function) irc_start, (Function) irc_close, (Function) irc_expmem,
(Function) irc_report, (Function) & H_splt, (Function) & H_rejn, (Function) & H_nick,
(Function) & H_sign, (Function) & H_join, (Function) & H_part, (Function) & H_mode,
(Function) & H_kick, (Function) & H_pubm, (Function) & H_pub, (Function) & H_topc,
(Function) recheck_channel, (Function) me_op, (Function) recheck_channel_modes, (Function) & H_need,
(Function) do_channel_part, (Function) check_this_ban, (Function) check_this_user,
(Function) me_voice, };
void
getin_describe (struct cfg_entry *cfgent, int idx)
{
} void
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
char *
irc_start (Function * global_funcs)
{
  struct chanset_t *chan;
  global = global_funcs;
  module_register (MODULE_NAME, irc_table, 1, 3);
  if (!(server_funcs = module_depend (MODULE_NAME, "server", 1, 0)))
    {
      module_undepend (MODULE_NAME);
      return "This module requires server module 1.0 or later.";
    }
  if (!(channels_funcs = module_depend (MODULE_NAME, "channels", 1, 0)))
    {
      module_undepend (MODULE_NAME);
      return "This module requires channels module 1.0 or later.";
    }
  if (!(encryption_funcs = module_depend (MODULE_NAME, "encryption", 0, 0)))
    {
      module_undepend (MODULE_NAME);
      return "This module requires an encryption modules.";
    }
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
  for (chan = chanset; chan; chan = chan->next)
    {
      if (!channel_inactive (chan))
	dprintf (DP_MODE, "JOIN %s %s\n",
		 (chan->name[0]) ? chan->name : chan->dname, chan->key_prot);
      chan->status &= ~(CHAN_ACTIVE | CHAN_PEND | CHAN_ASKEDBANS);
#ifdef S_IRCNET
      chan->ircnet_status &= ~(CHAN_ASKED_INVITED | CHAN_ASKED_EXEMPTS);
#endif
    }
  add_hook (HOOK_MINUTELY, (Function) check_expired_chanstuff);
  add_hook (HOOK_MINUTELY, (Function) check_servers);
  add_hook (HOOK_ADD_MODE, (Function) real_add_mode);
  add_hook (HOOK_IDLE, (Function) flush_modes);
  add_hook (HOOK_3SECONDLY, (Function) getin_3secondly);
  add_hook (HOOK_10SECONDLY, (Function) irc_10secondly);
#ifdef G_AUTOLOCK
  add_hook (HOOK_MINUTELY, (Function) check_netfight);
#endif
  Tcl_TraceVar (interp, "net-type",
		TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		traced_nettype, NULL);
  Tcl_TraceVar (interp, "rfc-compliant",
		TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		traced_rfccompliant, NULL);
  add_tcl_ints (myints);
  add_builtins (H_bot, irc_bot);
  add_builtins_dcc (H_dcc, irc_dcc);
  add_builtins (H_msg, C_msg);
  add_builtins (H_raw, irc_raw);
  add_tcl_commands (tclchan_cmds);
  H_topc = add_bind_table ("topc", HT_STACKABLE, channels_5char);
  H_splt = add_bind_table ("splt", HT_STACKABLE, channels_4char);
  H_sign = add_bind_table ("sign", HT_STACKABLE, channels_5char);
  H_rejn = add_bind_table ("rejn", HT_STACKABLE, channels_4char);
  H_part = add_bind_table ("part", HT_STACKABLE, channels_5char);
  H_nick = add_bind_table ("nick", HT_STACKABLE, channels_5char);
  H_mode = add_bind_table ("mode", HT_STACKABLE, channels_6char);
  H_kick = add_bind_table ("kick", HT_STACKABLE, channels_6char);
  H_join = add_bind_table ("join", HT_STACKABLE, channels_4char);
  H_pubm = add_bind_table ("pubm", HT_STACKABLE, channels_5char);
  H_pub = add_bind_table ("pub", 0, channels_5char);
  H_need = add_bind_table ("need", HT_STACKABLE, channels_2char);
  do_nettype ();
  return NULL;
}
#endif
