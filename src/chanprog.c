/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2008 Bryan Drewery
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
 * chanprog.c -- handles:
 *   rmspace()
 *   maintaining the server list
 *   revenge punishment
 *   timers, utimers
 *   telling the current programmed settings
 *   initializing a lot of stuff and loading the tcl scripts
 *
 */


#include "common.h"
#include "chanprog.h"
#include "settings.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/server.mod/server.h"
#include "src/mod/share.mod/share.h"
#include "rfc1459.h"
#include "net.h"
#include "misc.h"
#include "users.h"
#include "botnet.h"
#include "userrec.h"
#include "main.h"
#include "debug.h"
#include "dccutil.h"
#include "botmsg.h"
#if HAVE_GETRUSAGE
#include <sys/resource.h>
#if HAVE_SYS_RUSAGE_H
#include <sys/rusage.h>
#endif
#endif
#include <sys/utsname.h>

struct chanset_t 	*chanset = NULL;	/* Channel list			*/
char 			admin[121] = "";	/* Admin info			*/
char			origbotnick[NICKLEN + 1] = "";	/* from -B (placed into conf.bot->nick .. for backup when conf is cleared */
char 			origbotname[NICKLEN + 1] = "";	/* Nick to regain */
char                    jupenick[NICKLEN] = "";
char 			botname[NICKLEN + 1] = "";	/* IRC nickname */
port_t     		my_port = 0;
bool			reset_chans = 0;

/* Remove leading and trailing whitespaces.
 */
void rmspace(char *s)
{
  if (!s || !*s)
    return;

  register char *p = NULL, *q = NULL;

  /* Remove trailing whitespaces. */
  for (q = s + strlen(s) - 1; q >= s && egg_isspace(*q); q--)
    ;
  *(q + 1) = 0;

  /* Remove leading whitespaces. */
  for (p = s; egg_isspace(*p); p++)
    ;

  if (p != s)
    memmove(s, p, q - p + 2);
}

/* Returns memberfields if the nick is in the member list.
 */
memberlist *ismember(struct chanset_t *chan, const char *nick)
{
  register memberlist	*x = NULL;

  if (chan && nick && nick[0])
    for (x = chan->channel.member; x && x->nick[0]; x = x->next)
      if (!rfc_casecmp(x->nick, nick))
        return x;
  return NULL;
}

/* Find a chanset by channel name as the server knows it (ie !ABCDEchannel)
 */
struct chanset_t *findchan(const char *name)
{
  register struct chanset_t	*chan = NULL;

  for (chan = chanset; chan; chan = chan->next)
    if (!rfc_casecmp(chan->name, name))
      return chan;
  return NULL;
}

/* Find a chanset by display name (ie !channel)
 */
struct chanset_t *findchan_by_dname(const char *name)
{
  register struct chanset_t	*chan = NULL;

  for (chan = chanset; chan; chan = chan->next)
    if (!rfc_casecmp(chan->dname, name))
      return chan;
  return NULL;
}

/*
 *    "caching" functions
 */

/* Shortcut for get_user_by_host -- might have user record in one
 * of the channel caches.
 */
struct userrec *check_chanlist(const char *host)
{
  char				*nick = NULL, *uhost = NULL, buf[UHOSTLEN] = "";
  register memberlist		*m = NULL;
  register struct chanset_t	*chan = NULL;

  strlcpy(buf, host, sizeof buf);
  uhost = buf;
  nick = splitnick(&uhost);
  for (chan = chanset; chan; chan = chan->next)
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) 
      if (!rfc_casecmp(nick, m->nick) && !egg_strcasecmp(uhost, m->userhost))
	return m->user;
  return NULL;
}

/* Shortcut for get_user_by_handle -- might have user record in channels
 */
struct userrec *check_chanlist_hand(const char *hand)
{
  register struct chanset_t	*chan = NULL;
  register memberlist		*m = NULL;

  for (chan = chanset; chan; chan = chan->next)
    for (m = chan->channel.member; m && m->nick[0]; m = m->next)
      if (m->user && !egg_strcasecmp(m->user->handle, hand))
	return m->user;
  return NULL;
}

/* Clear the user pointers in the chanlists.
 *
 * Necessary when a hostmask is added/removed, a user is added or a new
 * userfile is loaded.
 */
void clear_chanlist(void)
{
  register memberlist		*m = NULL;
  register struct chanset_t	*chan = NULL;

  for (chan = chanset; chan; chan = chan->next)
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      m->user = NULL;
      m->tried_getuser = 0;
    }

}

/* Clear the user pointer of a specific nick in the chanlists.
 *
 * Necessary when a hostmask is added/removed, a nick changes, etc.
 * Does not completely invalidate the channel cache like clear_chanlist().
 */
void clear_chanlist_member(const char *nick)
{
  register memberlist		*m = NULL;
  register struct chanset_t	*chan = NULL;

  for (chan = chanset; chan; chan = chan->next)
    for (m = chan->channel.member; m && m->nick[0]; m = m->next)
      if (!rfc_casecmp(m->nick, nick)) {
	m->user = NULL;
        m->tried_getuser = 0;
	break;
      }
}

/* If this user@host is in a channel, set it (it was null)
 */
void set_chanlist(const char *host, struct userrec *rec)
{
  char				*nick = NULL, *uhost = NULL, buf[UHOSTLEN] = "";
  register memberlist		*m = NULL;
  register struct chanset_t	*chan = NULL;

  strlcpy(buf, host, sizeof buf);
  uhost = buf;
  nick = splitnick(&uhost);
  for (chan = chanset; chan; chan = chan->next)
    for (m = chan->channel.member; m && m->nick[0]; m = m->next)
      if (!rfc_casecmp(nick, m->nick) && !egg_strcasecmp(uhost, m->userhost))
	m->user = rec;
}

/* 0 marks all channels
 * 1 removes marked channels
 * 2 unmarks all channels
 */

void checkchans(int which)
{
  struct chanset_t *chan = NULL, *chan_next = NULL;

  if (which == 0 || which == 2) {
    for (chan = chanset; chan; chan = chan->next) {
      if (which == 0) {
        chan->status |= CHAN_FLAGGED;
      } else if (which == 2) {
        chan->status &= ~CHAN_FLAGGED;
      }
    }
  } else if (which == 1) {
    for (chan = chanset; chan; chan = chan_next) {
      chan_next = chan->next;
      if (chan->status & CHAN_FLAGGED) {
        putlog(LOG_MISC, "*", "No longer supporting channel %s", chan->dname);
        remove_channel(chan);
      }
    }
  }

}

void tell_verbose_uptime(int idx)
{
  char s[256] = "", s1[121] = "", s2[81] = "", outbuf[501] = "";
  time_t total, hr, min;
#if HAVE_GETRUSAGE
  struct rusage ru;
#else
# if HAVE_CLOCK
  clock_t cl;
# endif
#endif /* HAVE_GETRUSAGE */

  daysdur(now, online_since, s, sizeof(s));

  if (backgrd)
    strlcpy(s1, "background", sizeof(s1));
  else {
    if (term_z)
      strlcpy(s1, "terminal mode", sizeof(s1));
    else
      strlcpy(s1, "log dump mode", sizeof(s1));
  }
  simple_snprintf(outbuf, sizeof(outbuf), "Online for %s", s);
  if (restart_time) {
    daysdur(now, restart_time, s, sizeof(s));
    size_t olen = strlen(outbuf);
    simple_snprintf(&outbuf[olen], sizeof(outbuf) - olen, " (%s %s)", restart_was_update ? "updated" : "restarted", s);
  }

#if HAVE_GETRUSAGE
  getrusage(RUSAGE_SELF, &ru);
  total = ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;
  hr = (int) (total / 60);
  min = (int) (total - (hr * 60));
  snprintf(s2, sizeof(s2), "CPU %02d:%02d (load avg %3.1f%%)", (int) hr, (int) min, 100.0 * ((float) total / (float) (now - online_since)));
#else
# if HAVE_CLOCK
  cl = (clock() / CLOCKS_PER_SEC);
  hr = (int) (cl / 60);
  min = (int) (cl - (hr * 60));
  snprintf(s2, sizeof(s2), "CPU %02d:%02d (load avg %3.1f%%)", (int) hr, (int) min,  100.0 * ((float) cl / (float) (now - online_since)));
# else
  simple_snprintf(s2, sizeof(s2), "CPU ???");
# endif
#endif /* HAVE_GETRUSAGE */
  dprintf(idx, "%s  (%s)  %s  cache hit %4.1f%%\n",
          outbuf, s1, s2,
          100.0 * ((float) cache_hit) / ((float) (cache_hit + cache_miss)));

}

/* Dump status info out to dcc
 */
void tell_verbose_status(int idx)
{
  char *vers_t = NULL, *uni_t = NULL;
  int i;
  struct utsname un;

  if (!uname(&un) < 0) {
    vers_t = " ";
    uni_t = "*unknown*";
  } else {
    vers_t = un.release;
    uni_t = un.sysname;
  }

  i = count_users(userlist);
  dprintf(idx, "I am %s, running %s:  %d user%s\n", conf.bot->nick, ver, i, i == 1 ? "" : "s");
  dprintf(idx, "my user: %s\n", conf.bot->u->handle);
  if (conf.bot->localhub)
    dprintf(idx, "I am a localhub.\n");
  if (conf.bot->hub && isupdatehub())
    dprintf(idx, "I am an update hub.\n");

  tell_verbose_uptime(idx);

  if (admin[0])
    dprintf(idx, "Admin: %s\n", admin);

  dprintf(idx, "OS: %s %s\n", uni_t, vers_t);
  dprintf(idx, "Running from: %s\n", binname);
  dprintf(idx, "uid: %s (%d) pid: %d homedir: %s\n", conf.username, conf.uid, mypid, conf.homedir);
  if (tempdir[0])
    dprintf(idx, "Tempdir     : %s\n", tempdir);
  if (conf.datadir)
    dprintf(idx, "Datadir     : %s\n", conf.datadir);
}

/* Show all internal state variables
 */
void tell_settings(int idx)
{
  char s[1024] = "";
  struct flag_record fr = {FR_GLOBAL, 0, 0, 0 };

  dprintf(idx, "Botnet Nickname: %s\n", conf.bot->nick);
  if (conf.bot->hub)
    dprintf(idx, "Userfile: %s   \n", userfile);
  dprintf(idx, "Directories:\n");
  dprintf(idx, "  Temp    : %s\n", tempdir);
  fr.global = default_flags;

  build_flags(s, &fr, NULL);
  dprintf(idx, "New users get flags [%s]\n", s);
  if (conf.bot->hub && owner[0])
    dprintf(idx, "Permanent owner(s): %s\n", owner);
  dprintf(idx, "Ignores last %d mins\n", ignore_time);
}

void reaffirm_owners()
{
  /* Make sure perm owners are +a */
  if (owner[0]) {
    char *q = owner, *p = strchr(q, ','), s[121] = "";
    struct userrec *u = NULL;

    while (p) {
      strlcpy(s, q, (p - q) + 1);
      rmspace(s);
      u = get_user_by_handle(userlist, s);
      if (u)
	u->flags = sanity_check(u->flags | USER_ADMIN, 0);
      q = p + 1;
      p = strchr(q, ',');
    }
    strlcpy(s, q, sizeof(s));
    rmspace(s);
    u = get_user_by_handle(userlist, s);
    if (u)
      u->flags = sanity_check(u->flags | USER_ADMIN, 0);
  }
}

bool is_hub(const char* nick) {
  char *p = settings.hubs, *p2 = NULL, hubbuf[HANDLEN + 1] ="";
  size_t len = 0;

  while (p && *p) {
    if ((p2 = strchr(p, ' '))) {

      len = p2 - p;
      strlcpy(hubbuf, p, len + 1);
      if (!egg_strncasecmp(nick, hubbuf, HANDLEN))
        return 1;
    }
    if ((p = strchr(p, ',')))
      p++;
  }
  return 0;
}

void load_internal_users()
{
  char *p = NULL, *ln = NULL, *hand = NULL, *ip = NULL, *port = NULL, *pass = NULL, *q = NULL;
  char *hosts = NULL, buf[2048] = "", *attr = NULL, tmp[51] = "";
  int i, hublevel = 0;
  struct bot_addr *bi = NULL;
  struct userrec *u = NULL;

  /* hubs */
  strlcpy(buf, settings.hubs, sizeof(buf));
  p = buf;
  while (p) {
    ln = p;
    p = strchr(p, ',');
    if (p)
      *p++ = 0;
    hand = ln;
    ip = NULL;
    port = NULL;
    u = NULL;
    for (i = 0; ln; i++) {
      switch (i) {
        case 0:
          hand = ln;
          break;
        case 1:
          ip = ln;
          break;
        case 2:
          port = ln;
          hublevel++;		/* We must increment this even if it is already added */
          if (!get_user_by_handle(userlist, hand)) {
            userlist = adduser(userlist, hand, "none", "-", USER_OP, 1);
            u = get_user_by_handle(userlist, hand);

            egg_snprintf(tmp, sizeof(tmp), "%li [internal]", (long)now);
            set_user(&USERENTRY_ADDED, u, tmp);

            bi = (struct bot_addr *) my_calloc(1, sizeof(struct bot_addr));

            bi->address = strdup(ip);
            bi->telnet_port = atoi(port) ? atoi(port) : 0;
            bi->relay_port = bi->telnet_port;
            bi->hublevel = hublevel;
            if (conf.bot->hub && (!bi->hublevel) && (!egg_strcasecmp(hand, conf.bot->nick)))
              bi->hublevel = 99;
            bi->uplink = (char *) my_calloc(1, 1);
            set_user(&USERENTRY_BOTADDR, u, bi);
            /* set_user(&USERENTRY_PASS, get_user_by_handle(userlist, hand), SALT2); */
          }
          break;
        default:
          break;
      }
      if (ln && (ln = strchr(ln, ' ')))
        *ln++ = 0;
    }
  }

  /* perm owners */
  owner[0] = 0;

  strlcpy(buf, settings.owners, sizeof(buf));
  p = buf;
  while (p) {
    ln = p;
    p = strchr(p, ',');
    if (p)
      *p++ = 0;
    hand = ln;
    pass = NULL;
    attr = NULL;
    hosts = NULL;
    for (i = 0; ln; i++) {
      switch (i) {
        case 0:
          hand = ln;
          break;
        case 1:
          pass = ln;

          if (ln && (ln = strchr(ln, ' ')))
            *ln++ = 0;

          hosts = ln;
          if (owner[0])
            strlcat(owner, ",", 121);
          strlcat(owner, hand, 121);
          if (!get_user_by_handle(userlist, hand)) {
            userlist = adduser(userlist, hand, "none", "-", USER_ADMIN | USER_OWNER | USER_MASTER | USER_OP | USER_PARTY | USER_HUBA | USER_CHUBA, 0);
            u = get_user_by_handle(userlist, hand);
            set_user(&USERENTRY_PASS, u, pass);
            egg_snprintf(tmp, sizeof(tmp), "%li [internal]", (long)now);
            set_user(&USERENTRY_ADDED, u, tmp);
            while (hosts) {
              char x[1024] = "";

              if ((ln = strchr(ln, ' ')))
                *ln++ = 0;

              if ((q = strchr(hosts, '!'))) {	/* skip over nick they provided ... */
                q++;
                if (*q == '*' || *q == '?')		/* ... and any '*' or '?' */
                  q++;
                hosts = q;
              }

              simple_snprintf(x, sizeof(x), "-telnet!%s", hosts);
              set_user(&USERENTRY_HOSTS, u, x);
              hosts = ln;
            }
          }
          break;
        default:
          break;
      }
      if (ln && (ln = strchr(ln, ' ')))
        *ln++ = 0;
    }
  }

}

void add_myself_to_userlist() {
  struct bot_addr *bi = NULL;

  if (!(conf.bot->u = get_user_by_handle(userlist, conf.bot->nick))) {
    /* I need to be on the userlist... doh. */
    userlist = adduser(userlist, conf.bot->nick, "none", "-", USER_OP, 1);
    conf.bot->u = get_user_by_handle(userlist, conf.bot->nick);
    bi = (struct bot_addr *) my_calloc(1, sizeof(struct bot_addr));

    /* Assume hub has a record added from load_internal_users();
       why would it think it was a hub if it wasn't in the hub list??
    */
    if (!conf.bot->hub) {
      if (conf.bot->net.ip)
        bi->address = strdup(conf.bot->net.ip);
      bi->telnet_port = bi->relay_port = 3333;
      bi->hublevel = 999;
      bi->uplink = (char *) my_calloc(1, 1);
      set_user(&USERENTRY_BOTADDR, conf.bot->u, bi);
    }
  }
}

void chanprog()
{
  struct utsname un;

  /* cache our ip on load instead of every 30 seconds */
  char *ip4 = NULL, *ip6 = NULL;

  if (cached_ip) {
    ip4 = strdup(myipstr(AF_INET));
    ip6 = strdup(myipstr(AF_INET6));
  }

  cache_my_ip();
  sdprintf("ip4: %s", myipstr(AF_INET));
  sdprintf("ip6: %s", myipstr(AF_INET6));

  /* Check if our ip changed during a rehash */
  if (ip4) {
    if (strcmp(ip4, myipstr(AF_INET)) || strcmp(ip6, myipstr(AF_INET6))) {
      if (tands > 0) {
        botnet_send_chat(-1, conf.bot->nick, "IP changed.");
        botnet_send_bye("IP changed.");
      }
      fatal("brb", 1);
    }

    free(ip4);
    free(ip6);
  }

  sdprintf("I am: %s", conf.bot->nick);
  if (conf.bot->hub) {
    simple_snprintf(userfile, 121, "%s/.u", conf.binpath);
    loading = 1;
    checkchans(0);
    readuserfile(userfile, &userlist);
    checkchans(1);
    var_parse_my_botset();
    loading = 0;
  }

  load_internal_users();

  add_myself_to_userlist();

  if (conf.bot->hub) {
    struct bot_addr *bi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, conf.bot->u);
    listen_all(bi->telnet_port, 0);
    my_port = bi->telnet_port;
  }

  /* set our shell info */
  uname(&un);
  set_user(&USERENTRY_OS, conf.bot->u, un.sysname);
  set_user(&USERENTRY_USERNAME, conf.bot->u, conf.username);
  set_user(&USERENTRY_NODENAME, conf.bot->u, un.nodename);
  set_user(&USERENTRY_ARCH, conf.bot->u, un.machine);
  set_user(&USERENTRY_OSVER, conf.bot->u, un.release);

  var_parse_my_botset();

  /* We should be safe now */

  reaffirm_owners();
}

/* Reload the user file from disk
 */
void reload()
{
  FILE *f = fopen(userfile, "r");

  if (f == NULL) {
    putlog(LOG_MISC, "*", "Can't reload user file!");
    return;
  }
  fclose(f);
  noshare = 1;
  clear_userlist(userlist);
  noshare = 0;
  userlist = NULL;
  loading = 1;
  checkchans(0);
  if (!readuserfile(userfile, &userlist))
    fatal("User file is missing!", 0);

  /* ensure we did not lose our internal users */
  load_internal_users();
  /* make sure I am added and conf.bot->u is set */
  add_myself_to_userlist();

  /* Make sure no removed users/bots are still connected. */
  check_stale_dcc_users();

  for (tand_t* bot = tandbot; bot; bot = bot->next)
    bot->u = get_user_by_handle(userlist, bot->bot);

  /* I don't think these will ever be called anyway. */
  if (!conf.bot->hub) {
    Auth::FillUsers();
    check_hostmask();
  }

  checkchans(1);
  loading = 0;
  var_parse_my_botset();
  reaffirm_owners();
  hook_read_userfile();
}

/* Oddly enough, written by proton (Emech's coder)
 */
int isowner(char *name)
{
  if (!owner[0])
    return (0);
  if (!name || !name[0])
    return (0);

  char *pa = owner, *pb = owner;
  size_t nl = strlen(name), pl;

  while (1) {
    while (1) {
      if ((*pb == 0) || (*pb == ',') || (*pb == ' '))
	break;
      pb++;
    }
    pl = pb - pa;
    if (pl == nl && !egg_strncasecmp(pa, name, nl))
      return (1);
    while (1) {
      if ((*pb == 0) || ((*pb != ',') && (*pb != ' ')))
	break;
      pb++;
    }
    if (*pb == 0)
      return (0);
    pa = pb;
  }
}

/* this method is slow, but is only called when sharing and adding/removing chans */

int 
botshouldjoin(struct userrec *u, struct chanset_t *chan)
{
  /* just return 1 for now */
  return 1;
/*
  char *chans = NULL;
  struct userrec *u = NULL;
 
  u = get_user_by_handle(userlist, bot);
  if (!u)
    return;
  chans = get_user(&USERENTRY_CHANS, u);
*/
}
/* future use ?
void
chans_addbot(const char *bot, struct chanset_t *chan)
{
  char *chans = NULL;
  struct userrec *u = NULL;
 
  u = get_user_by_handle(userlist, bot);
  if (!u)
    return;
  chans = get_user(&USERENTRY_CHANS, u);
  if (!botshouldjoin(u, chan)) {		
    size_t size;
    char *buf = NULL;
   
    size = strlen(chans) + strlen(chan->dname) + 2;
    buf = (char *) my_calloc(1, size);
    simple_snprintf(buf, size, "%s %s", chans, chan->dname);
    set_user(&USERENTRY_CHANS, u, buf);
    free(buf);
  }
}

void 
chans_delbot(const char *bot, struct chanset_t *chan)
{
  char *chans = NULL;
  struct userrec *u = NULL;
 
  u = get_user_by_handle(userlist, bot);
  if (!u)
    return;
 
  if (botshouldjoin(u, chan)) {			
    char *chans = NULL, *buf = NULL;
    size_t size;

    chans = get_user(&USERENTRY_CHANS, u);
    size = strlen(chans) - strlen(chan->dname) + 2;

    
  }
}
*/

bool bot_shouldjoin(struct userrec* u, struct flag_record* fr, struct chanset_t* chan)
{
  /* If the bot is restarting (and hasn't finished getting the userfile for the first time) DO NOT JOIN channels - breaks +B/+backup */
  if (restarting || loading) return 0;

  /* Force debugging bots to only join 3 channels */
  if (!strncmp(u->handle, STR("wtest"), 5)) {
    if (!strcmp(chan->dname, STR("#skynet")) || 
        !strcmp(chan->dname, STR("#bryan")) || 
        !strcmp(chan->dname, STR("#wraith")))
      return 1;
    else
      return 0;
  }
  return (!channel_inactive(chan) && (channel_backup(chan) || (!glob_backup(*fr) && !chan_backup(*fr))));
}

bool shouldjoin(struct chanset_t *chan)
{
  if (conf.bot->u) {
    struct flag_record fr = { FR_CHAN|FR_GLOBAL|FR_BOT, 0, 0, 0 };
    get_user_flagrec(conf.bot->u, &fr, chan->dname, chan);
    return bot_shouldjoin(conf.bot->u, &fr, chan);
  }
  return 0;
}

/* do_chanset() set (options) on (chan)
 * USES DO_LOCAL|DO_NET bits.
 */
int do_chanset(char *result, struct chanset_t *chan, const char *options, int local)
{
  int ret = OK;

  if (local & DO_NET) {
    size_t bufsiz = 0;
         /* malloc(options,chan,'cset ',' ',+ 1) */
    if (chan)
      bufsiz = strlen(options) + strlen(chan->dname) + 5 + 1 + 1;
    else
      bufsiz = strlen(options) + 1 + 5 + 1 + 1;
    
    char *buf = (char*) my_calloc(1, bufsiz);

    strlcat(buf, "cset ", bufsiz);
    if (chan)
      strlcat(buf, chan->dname, bufsiz);
    else
      strlcat(buf, "*", bufsiz);
    strlcat(buf, " ", bufsiz);
    strlcat(buf, options, bufsiz);
    putlog(LOG_DEBUG, "*", "sending out cset: %s", buf);
    putallbots(buf); 
    free(buf);
  }

  if (local & DO_LOCAL) {
    bool cmd = (local & CMD);
    struct chanset_t *ch = NULL;
    int all = chan ? 0 : 1;

    if (chan)
      ch = chan;
    else
      ch = chanset;

    while (ch) {
      const char **item = NULL;
      int items = 0;

      if (SplitList(result, options, &items, &item) == OK) {
        ret = channel_modify(result, ch, items, (char **) item, cmd);
      } else 
        ret = ERROR;


      free(item);

      if (all) {
        if (ret == ERROR) /* just bail if there was an error, no sense in trying more */
          return ret;

        ch = ch->next;
      } else {
        ch = NULL;
      }
    }
  }
  return ret;
}

char *
samechans(const char *nick, const char *delim)
{
  static char ret[1024] = "";
  struct chanset_t *chan = NULL;

  ret[0] = 0;		/* may be filled from last time */
  for (chan = chanset; chan; chan = chan->next) {
    if (ismember(chan, nick)) {
      strlcat(ret, chan->dname, sizeof(ret));
      strlcat(ret, delim, sizeof(ret));
    }
  }
  ret[strlen(ret) - 1] = 0;

  return ret;
}
