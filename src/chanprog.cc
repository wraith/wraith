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
 * chanprog.c -- handles:
 *   rmspace()
 *   maintaining the server list
 *   timers, utimers
 *   telling the current programmed settings
 *   initializing a lot of stuff and loading the tcl scripts
 *
 */


#include "common.h"
#include "chanprog.h"
#include "settings.h"
#include "src/mod/irc.mod/irc.h"
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
#include "set.h"
#include "dccutil.h"
#include "botmsg.h"
#if HAVE_GETRUSAGE
#include <sys/resource.h>
#if HAVE_SYS_RUSAGE_H
#include <sys/rusage.h>
#endif
#endif
#include <sys/utsname.h>
#include <bdlib/src/Array.h>
#include <bdlib/src/HashTable.h>
#include <bdlib/src/String.h>

bd::HashTable<RfcString, struct chanset_t *> chanset_by_dname;

char *def_chanset = "+enforcebans +dynamicbans +userbans -bitch +cycle -inactive +userexempts -dynamicexempts +userinvites -dynamicinvites -nodesynch -closed -take -voice -private -fastop ban-type 3 protect-backup 1 groups { main } revenge react";
struct chanset_t 	*chanset = NULL;	/* Channel list			*/
struct chanset_t	*chanset_default = NULL;	/* Default channel list */
char 			admin[121] = "";	/* Admin info			*/
char			origbotnick[HANDLEN + 1] = "";	/* from -B (placed into conf.bot->nick .. for backup when conf is cleared */
char 			origbotname[NICKLEN] = "";	/* Nick to regain */
char                    jupenick[NICKLEN] = "";
char 			botname[NICKLEN] = "";	/* IRC nickname */
in_port_t     		my_port = 0;
int			reset_chans = 0;
bool                    cookies_disabled = 0;
char s2_4[3] = "",s1_6[3] = "",s1_11[3] = "";

/* Remove leading and trailing whitespaces.
 */
void rmspace(char *s)
{
  if (!s || !*s)
    return;

  char *p = NULL, *q = NULL;

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

/* Find a chanset by channel name as the server knows it (ie !ABCDEchannel)
 */
struct chanset_t *findchan(const char *name)
{
  struct chanset_t	*chan = NULL;

  for (chan = chanset; chan; chan = chan->next)
    if (chan->name[0] && !rfc_casecmp(chan->name, name))
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
  const memberlist		*m = NULL;
  const struct chanset_t	*chan = NULL;

  strlcpy(buf, host, sizeof buf);
  uhost = buf;
  nick = splitnick(&uhost);
  for (chan = chanset; chan; chan = chan->next)
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) 
      if (!rfc_casecmp(nick, m->nick) && !strcasecmp(uhost, m->userhost))
	return m->user;
  return NULL;
}

/* Shortcut for get_user_by_handle -- might have user record in channels
 */
struct userrec *check_chanlist_hand(const char *hand)
{
  const struct chanset_t	*chan = NULL;
  const memberlist		*m = NULL;

  for (chan = chanset; chan; chan = chan->next)
    for (m = chan->channel.member; m && m->nick[0]; m = m->next)
      if (m->user && !strcasecmp(m->user->handle, hand))
	return m->user;
  return NULL;
}

/* Clear the user pointer of a specific nick in the chanlists.
 *
 * Necessary when a hostmask is added/removed, a nick changes, etc.
 * Does not completely invalidate the channel cache like clear_chanlist().
 */
void clear_chanlist_member(const char *nick)
{
  memberlist		*m = NULL;
  struct chanset_t	*chan = NULL;

  for (chan = chanset; chan; chan = chan->next) {
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      if (nick == NULL || !rfc_casecmp(m->nick, nick)) {
	m->user = NULL;
        m->tried_getuser = 0;
        if (nick != NULL) {
          break;
        }
      }
    }
  }

  Auth::NullUsers(nick);
}

/* If this user@host is in a channel, set it (it was null)
 */
void set_chanlist(const char *host, struct userrec *rec)
{
  char				*nick = NULL, *uhost = NULL, buf[UHOSTLEN] = "";
  memberlist		*m = NULL;
  struct chanset_t	*chan = NULL;

  strlcpy(buf, host, sizeof buf);
  uhost = buf;
  nick = splitnick(&uhost);
  for (chan = chanset; chan; chan = chan->next)
    for (m = chan->channel.member; m && m->nick[0]; m = m->next)
      if (!rfc_casecmp(nick, m->nick) && !strcasecmp(uhost, m->userhost))
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
  clock_t cl;
#endif /* HAVE_GETRUSAGE */

  daysdur(now, online_since, s, sizeof(s), false);

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
    daysdur(now, restart_time, s, sizeof(s), false);
    size_t olen = strlen(outbuf);
    simple_snprintf(&outbuf[olen], sizeof(outbuf) - olen, " (%s %s ago)", restart_was_update ? "updated" : "restarted", s);
  }

#if HAVE_GETRUSAGE
  getrusage(RUSAGE_SELF, &ru);
  total = ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;
  hr = (int) (total / 60);
  min = (int) (total - (hr * 60));
  egg_snprintf(s2, sizeof(s2), "CPU %02d:%02d (load avg %3.1f%%)", (int) hr, (int) min, 100.0 * ((float) total / (float) (now - online_since)));
#else
  cl = (clock() / CLOCKS_PER_SEC);
  hr = (int) (cl / 60);
  min = (int) (cl - (hr * 60));
  egg_snprintf(s2, sizeof(s2), "CPU %02d:%02d (load avg %3.1f%%)", (int) hr, (int) min,  100.0 * ((float) cl / (float) (now - online_since)));
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

  if (uname(&un) < 0) {
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
  dprintf(idx, "uid: %s (%d) pid: %lu homedir: %s\n", conf.username, conf.uid, (unsigned long) mypid, conf.homedir);
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
  bd::String hub(size_t(HANDLEN));

  for (size_t idx = 0; idx < conf.hubs.length(); ++idx) {
    hub = conf.hubs[idx];
    if (!strncasecmp(nick, newsplit(hub).c_str(), HANDLEN)) {
      return true;
    }
  }
  return false;
}

void load_internal_users()
{
  char *p = NULL, *ln = NULL, *hand = NULL, *pass = NULL, *q = NULL;
  char *hosts = NULL, buf[2048] = "", tmp[51] = "";
  int i;
  struct userrec *u = NULL;

  /* hubs */
  for (size_t idx = 0; idx < conf.hubs.length(); ++idx) {
    bd::Array<bd::String> params(static_cast<bd::String>(conf.hubs[idx]).split(' '));
    const bd::String handle(params[0]);
    const bd::String address(params[1]);
    const in_port_t port = atoi(static_cast<bd::String>(params[2]).c_str());
    const unsigned short hublevel = params.length() == 4 ? atoi(static_cast<bd::String>(params[3]).c_str()) : (idx + 1);

    if (!(u = get_user_by_handle(userlist, const_cast<char*>(handle.c_str())))) {
      userlist = adduser(userlist, handle.c_str(), "none", "-", USER_OP, 1);
      u = get_user_by_handle(userlist, const_cast<char*>(handle.c_str()));
    }

    simple_snprintf(tmp, sizeof(tmp), "%li [internal]", (long)now);
    set_user(&USERENTRY_ADDED, u, tmp);

    struct bot_addr *bi = (struct bot_addr *) calloc(1, sizeof(struct bot_addr));

    bi->address = strdup(address.c_str());
    bi->telnet_port = bi->relay_port = port;
    bi->hublevel = hublevel;
    if (conf.bot->hub && (!bi->hublevel) && (!strcasecmp(handle.c_str(), conf.bot->nick))) {
      bi->hublevel = 99;
    }
    bi->uplink = (char *) calloc(1, 1);
    set_user(&USERENTRY_BOTADDR, u, bi);
    /* set_user(&USERENTRY_PASS, get_user_by_handle(userlist, handle.c_str()), SALT2); */
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
            simple_snprintf(tmp, sizeof(tmp), "%li [internal]", (long)now);
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

  // Add HQ in if needed
  if (!backgrd && term_z) {
    strlcat(owner, ",HQ", sizeof(owner));
  }

}

static struct userrec* add_bot_userlist(char* bot) {
  struct userrec *u = NULL;
  if (!(u = get_user_by_handle(userlist, bot))) {
    /* I need to be on the userlist... doh. */
    userlist = adduser(userlist, bot, "none", "-", USER_OP, 1);
    u = get_user_by_handle(userlist, bot);

    struct bot_addr *bi = (struct bot_addr *) calloc(1, sizeof(struct bot_addr));
    bi->uplink = (char *) calloc(1, 1);
    bi->address = (char *) calloc(1, 1);
    bi->telnet_port = 3333;
    bi->relay_port = 3333;
    bi->hublevel = 999;
    set_user(&USERENTRY_BOTADDR, u, bi);
  }
  return u;
}

void add_myself_to_userlist() {
  char buf[15];

  conf.bot->u = add_bot_userlist(conf.bot->nick);

  simple_snprintf(buf, sizeof(buf), "%d", ALL_FEATURE_FLAGS);
  set_user(&USERENTRY_FFLAGS, conf.bot->u, buf);
}

void add_child_bots() {
  conf_bot* bot = conf.bots->next; //Skip myself
  if (bot && bot->nick) {
    for (; bot && bot->nick; bot = bot->next) {
      add_bot_userlist(bot->nick);
    }
  }
}

void add_localhub() {
  add_bot_userlist(conf.localhub);
}

void rehash_ip() {
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

  if (conf.bot->hub) {
    struct bot_addr *bi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, conf.bot->u);
    listen_all(bi->telnet_port, 0, 1);
    my_port = bi->telnet_port;
  } else if (conf.bot->localhub) {
    // If not listening on the domain socket, open it up
    bool listening = 0;
    for (int i = 0; i < dcc_total; i++) {
      if (dcc[i].type && (dcc[i].type == &DCC_TELNET) && (!strcmp(dcc[i].host, conf.localhub_socket)) && (!strcmp(dcc[i].nick, "(unix_domain"))) {
        listening = 1;
        break;
      }
    }
    if (!listening) {
      // Listen on the unix domain socket
      in_port_t port;
      int i = open_listen_addr_by_af(conf.localhub_socket, &port, AF_UNIX);
      if (i < 0) {
        putlog(LOG_ERRORS, "*", "Can't listen on %s - %s", conf.localhub_socket, i == -1 ? "it's taken." : "couldn't assign file.");
      } else {
        /* now setup dcc entry */
        int idx = new_dcc(&DCC_TELNET, 0);
        dcc[idx].addr = 0L;
        strlcpy(dcc[idx].host, conf.localhub_socket, sizeof(dcc[idx].host));
        dcc[idx].port = 0;
        dcc[idx].sock = i;
        dcc[idx].timeval = now;
        strlcpy(dcc[idx].nick, "(unix_domain)", sizeof(dcc[idx].nick));
        putlog(LOG_DEBUG, "*", "Listening on telnet %s", conf.localhub_socket);
      }
    }
  }
}

void chanprog()
{
  struct utsname un;

  sdprintf("I am: %s", conf.bot->nick);

  /* Add the 'default' virtual channel.
     The flags will be overwritten when the userfile is loaded,
     but if there is no userfile yet, or no 'default' channel,
     then it will take on the properties of 'def_chanset',
     and then later save this to the userfile.
   */
  channel_add(NULL, "default", def_chanset, 1);

  if (conf.bot->hub) {
    simple_snprintf(userfile, 121, "%s/.u", dirname(binname));
    loading = 1;
    checkchans(0);
    readuserfile(userfile, &userlist);
    checkchans(1);
    var_parse_my_botset();
    loading = 0;
  }

  load_internal_users();

  add_myself_to_userlist();

  if (conf.bot->localhub)
    add_child_bots();
  else if (!conf.bot->hub)
    add_localhub();

  rehash_ip();

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

  if (conf.bot->localhub)
    add_child_bots();
  else if (!conf.bot->hub)
    add_localhub();

  cache_users();

  /* Make sure no removed users/bots are still connected. */
  check_stale_dcc_users();

  /* I don't think these will ever be called anyway. */
  if (!conf.bot->hub) {
    check_hostmask();
  }

  checkchans(1);
  loading = 0;
  var_parse_my_botset();
  reaffirm_owners();
  hook_read_userfile();
}

void setup_HQ(int n) {
    dcc[n].addr = iptolong(getmyip());
    dcc[n].sock = STDOUT;
    dcc[n].timeval = now;
    dcc[n].u.chat->con_flags = conmask;
    dcc[n].u.chat->strip_flags = STRIP_ALL;
    dcc[n].status = STAT_ECHO|STAT_COLOR;
    strlcpy(dcc[n].nick, STR("HQ"), sizeof(dcc[n].nick));
    strlcpy(dcc[n].host, STR("llama@console"), sizeof(dcc[n].host));
    dcc[n].user = get_user_by_handle(userlist, dcc[n].nick);
    /* Make sure there's an innocuous HQ user if needed */
    if (!dcc[n].user) {
      userlist = adduser(userlist, dcc[n].nick, "none", "-", USER_ADMIN | USER_OWNER | USER_MASTER | USER_VOICE | USER_OP | USER_PARTY | USER_CHUBA | USER_HUBA, 0);
      dcc[n].user = get_user_by_handle(userlist, dcc[n].nick);
    }
}

/* Oddly enough, written by proton (Emech's coder)
 */
int isowner(const char *name)
{
  if (!owner[0])
    return (0);
  if (!name || !name[0])
    return (0);

  const char *pa = owner, *pb = owner;
  size_t nl = strlen(name), pl;

  while (1) {
    while (1) {
      if ((*pb == 0) || (*pb == ',') || (*pb == ' '))
	break;
      pb++;
    }
    pl = pb - pa;
    if (pl == nl && !strncasecmp(pa, name, nl))
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

bool bot_shouldjoin(struct userrec* u, const struct flag_record* fr,
    const struct chanset_t* chan, bool ignore_inactive)
{
  // If restarting, keep this channel.
  if (restarting && (reset_chans == 2) && (channel_active(chan) || channel_pending(chan))) return 1;
  /* If the bot is restarting (and hasn't finished getting the userfile for the first time) DO NOT JOIN channels - breaks +B/+backup */
  if (restarting || loading) return 0;

  // No user record, can't make any safe assumptions really
  if (!u) return 0;

  // Is this bot in the groups that this channel has?
  const char *botgroups = u == conf.bot->u ? groups : var_get_bot_data(u, "groups", true);
  bd::Array<bd::String> my_groupsArray(bd::String(botgroups).split(','));
  bool group_match = 0;

  if (chan->groups && chan->groups->length()) {
    for (size_t i = 0; i < my_groupsArray.length(); ++i) {
      if (chan->groups->find(my_groupsArray[i]) != chan->groups->npos) {
        group_match = 1;
        break;
      }
    }
  }

  // Ignore +inactive during cmd_slowjoin to ensure that +backup bots join
  return (!glob_kick(*fr) && !chan_kick(*fr) && // Not being kicked
      ((ignore_inactive || !channel_inactive(chan)) && // Not inactive
      ((channel_backup(chan) && (glob_backup(*fr) || chan_backup(*fr) || group_match)) || // Is +backup and I'm a backup bot or my group matches
       (!channel_backup(chan) && !glob_backup(*fr) && !chan_backup(*fr) && group_match))) // is -backup and I am not a backup bot and my group matches
      );
}

bool shouldjoin(const struct chanset_t *chan)
{
  struct flag_record fr = { FR_CHAN|FR_GLOBAL|FR_BOT, 0, 0, 0 };
  get_user_flagrec(conf.bot->u, &fr, chan->dname, chan);
  return bot_shouldjoin(conf.bot->u, &fr, chan);
}

/* do_chanset() set (options) on (chan)
 * USES DO_LOCAL|DO_NET bits.
 */
int do_chanset(char *result, struct chanset_t *chan, const char *options, int flags)
{
  int ret = OK;

  if (flags & DO_NET) {
    size_t bufsiz = 0;
         /* malloc(options,chan,'cset ',' ',+ 1) */
    if (chan)
      bufsiz = strlen(options) + strlen(chan->dname) + 5 + 1 + 1;
    else
      bufsiz = strlen(options) + 1 + 5 + 1 + 1;
    
    char *buf = (char*) calloc(1, bufsiz);

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

  if (flags & DO_LOCAL) {
    bool cmd = (flags & CMD);
    struct chanset_t *ch = NULL;
    int all = chan ? 0 : 1;

    if (chan)
      ch = chan;
    else
      ch = chanset_default; //First iteration changes default, then move on to all chans

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

        if (ch == chanset_default)
          ch = chanset;
        else
          ch = ch->next;
      } else {
        ch = NULL;
      }
    }
  }
  return ret;
}

const char *
samechans(const char *nick, const char *delim)
{
  static char ret[1024] = "";
  const struct chanset_t *chan = NULL;

  ret[0] = 0;		/* may be filled from last time */
  for (chan = chanset; chan; chan = chan->next) {
    if (ismember(chan, nick)) {
      strlcat(ret, chan->dname, sizeof(ret));
      strlcat(ret, delim, sizeof(ret));
    }
  }

  return ret;
}

static struct chanset_t*
__attribute__((pure))
find_common_opped_chan(const bd::String& nick) {
  for (struct chanset_t* chan = chanset; chan; chan = chan->next) {
    if (channel_active(chan) && (me_op(chan) || me_voice(chan))) {
      if (ismember(chan, nick.c_str()))
        return chan;
    }
  }
  return NULL;
}

void privmsg(const bd::String& target, bd::String msg, int idx) {
  struct chanset_t* chan = NULL;
  bool talking_to_chan = strchr(CHANMETA, target[0]);
  if (have_cprivmsg && !talking_to_chan)
    chan = find_common_opped_chan(target);
  bool cleartextPrefix = (msg(0, 3) == "+p ");

  // Encrypt with FiSH?
  if (!cleartextPrefix && FishKeys.contains(target) && FishKeys[target]->sharedKey.length()) {
    msg = "+OK " + egg_bf_encrypt(msg, FishKeys[target]->sharedKey);
  }

  if (cleartextPrefix) {
    msg += static_cast<size_t>(3);
  }

  if (chan)
    dprintf(idx, "CPRIVMSG %s %s :%s\n", target.c_str(), chan->name, msg.c_str());
  else
    dprintf(idx, "PRIVMSG %s :%s\n", target.c_str(), msg.c_str());
}

void notice(const bd::String& target, bd::String msg, int idx) {
  struct chanset_t* chan = NULL;
  bool talking_to_chan = strchr(CHANMETA, target[0]);
  if (have_cnotice && !talking_to_chan)
    chan = find_common_opped_chan(target);
  bool cleartextPrefix = (msg(0, 3) == "+p ");

  if (!cleartextPrefix && FishKeys.contains(target) && FishKeys[target]->sharedKey.length()) {
    msg = "+OK " + egg_bf_encrypt(msg, FishKeys[target]->sharedKey);
  }

  if (cleartextPrefix) {
    msg += static_cast<size_t>(3);
  }

  if (chan)
    dprintf(idx, "CNOTICE %s %s :%s\n", target.c_str(), chan->name, msg.c_str());
  else
    dprintf(idx, "NOTICE %s :%s\n", target.c_str(), msg.c_str());
}


void keyx(const bd::String &target, const char *reason) {
  bd::String myPublicKeyB64, myPrivateKey, sharedKey;

  DH1080_gen(myPrivateKey, myPublicKeyB64);

  fish_data_t* fishData = FishKeys.contains(target) ? FishKeys[target] : new fish_data_t;
  fishData->sharedKey.clear();
  putlog(LOG_MSGS, "*", "[FiSH] Initiating DH1080 key-exchange with %s - "
      "sending my public key (%s)", target.c_str(), reason);
  notice(target, "DH1080_INIT " + myPublicKeyB64, DP_HELP);
  fishData->myPublicKeyB64 = myPublicKeyB64;
  fishData->myPrivateKey = myPrivateKey;
  fishData->key_created_at = now;
  FishKeys[target] = fishData;
}

void set_fish_key(const char *target, const bd::String key)
{
  fish_data_t* fishData = FishKeys.contains(target) ? FishKeys[target] : NULL;

  if (!key.length()) { //remove key
    if (fishData) {
      FishKeys.remove(target);
      delete fishData;
    }
  } else { //set key
    fishData = new fish_data_t;

    if (key == "rand") {
      // Set a RANDOM key
      const size_t randomKeyLength = 32;
      char *rand_key = (char*)calloc(1, randomKeyLength+1);
      make_rand_str(rand_key, randomKeyLength);
      fishData->sharedKey = rand_key;
      free(rand_key);
    } else {
      fishData->sharedKey = key;
    }

    // Set the key
    FishKeys[target] = fishData;
  }
}
/* vim: set sts=2 sw=2 ts=8 et: */
