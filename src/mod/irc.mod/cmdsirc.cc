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
 * chancmds.c -- part of irc.mod
 *   handles commands directly relating to channel interaction
 *
 */

#include <bdlib/src/Stream.h>
#include <bdlib/src/String.h>
#include <algorithm>
using std::swap;
#include "src/misc_file.h"

/* Do we have any flags that will allow us ops on a channel?
 */
static struct chanset_t *get_channel(int idx, char *chname, bool check_console = 1, bool* all = NULL)
{
  struct chanset_t *chan = NULL;

  if (all && ((chname && chname[0] && !strcmp(chname, "*")) || (check_console && !(chname && chname[0]) && !strcmp(dcc[idx].u.chat->con_chan, "*")))) {
    *all = 1;
    return NULL;
  }

  if (chname && chname[0]) {
    chan = findchan_by_dname(chname);
    if (chan)
      return chan;
    else
      dprintf(idx, "No such channel.\n");
  } else if (check_console) {
    chname = dcc[idx].u.chat->con_chan;
    chan = findchan_by_dname(chname);
    if (chan)
      return chan;
    else
      dprintf(idx, "Invalid console channel.\n");
  }
  return NULL;
}

/* Do we have any flags that will allow us ops on a channel?
 */
static int has_op(int idx, struct chanset_t *chan)
{
  get_user_flagrec(dcc[idx].user, &user, chan->dname);
  if (privchan(user, chan, PRIV_OP)) {
    dprintf(idx, "No such channel.\n");
    return 0;
  }
  if (real_chk_op(user, chan, 0))
    return 1;
  dprintf(idx, "You are not a channel op on %s.\n", chan->dname);
  return 0;
}


/* Finds a nick of the handle. Returns m->nick if
 * the nick was found, otherwise NULL (Sup 1Nov2000)
 */
char *getnick(const char *handle, struct chanset_t *chan)
{
  for (memberlist *m = chan->channel.member; m && m->nick[0]; m = m->next) {
    member_getuser(m);
    if (m->user && !strcasecmp(m->user->handle, handle))
      return m->nick;
  }
  return "";
}

static void cmd_act(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: act [channel] <action>\n");
    return;
  }

  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (strchr(CHANMETA, par[0]) != NULL)
    chname = newsplit(&par);
  else
    chname = 0;
  chan = get_channel(idx, chname);
  if (!chan || !has_op(idx, chan))
    return;

  memberlist *m = ismember(chan, botname);

  if (!m) {
    dprintf(idx, "Cannot say to %s: I'm not on that channel.\n", chan->dname);
    return;
  }

  get_user_flagrec(dcc[idx].user, &user, chan->dname);

  if ((chan->channel.mode & CHANMODER) && !me_op(chan) && !me_voice(chan)) {
    dprintf(idx, "Cannot say to %s: It is moderated.\n", chan->dname);
    return;
  }
  putlog(LOG_CMDS, "*", "#%s# (%s) act %s", dcc[idx].nick,
	 chan->dname, par);
  bd::String msg;
  msg = bd::String::printf("\001ACTION %s\001", par);
  privmsg(chan->name, msg, DP_HELP);
  dprintf(idx, "Action to %s: %s\n", chan->dname, par);
}

static void cmd_msg(int idx, char *par)
{
  if (dcc[idx].simul >= 0) {
    dprintf(idx, "Sorry, that cmd isn't available over botcmd.\n");
    dprintf(idx, "Instead try: %sbotmsg %s %s\n", settings.dcc_prefix, conf.bot->nick, par);
    return;
  }

  if (!par[0]) {
    dprintf(idx, "Usage: msg <nick> <message>\n");
    return;
  }

  char *nick = newsplit(&par);

  putlog(LOG_CMDS, "*", "#%s# msg %s %s", dcc[idx].nick, nick, par);
  privmsg(nick, par, DP_HELP);
  dprintf(idx, "Msg to %s: %s\n", nick, par);
}

static void cmd_nick(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# nick", dcc[idx].nick);
  botnet_send_cmd(conf.bot->nick, conf.bot->nick, dcc[idx].nick, idx, "curnick");
}

static void cmd_say(int idx, char *par)
{
  if (dcc[idx].simul >= 0) {
    dprintf(idx, "Sorry, that cmd isn't available over botcmd.\n");
    dprintf(idx, "Instead try: %sbotmsg %s %s\n", settings.dcc_prefix, conf.bot->nick, par);
    return;
  }

  if (!par[0]) {
    dprintf(idx, "Usage: say [channel] <message>\n");
    return;
  }

  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (strchr(CHANMETA, par[0]) != NULL)
    chname = newsplit(&par);
  else
    chname = 0;
  chan = get_channel(idx, chname);
  if (!chan || !has_op(idx, chan))
    return;

  memberlist *m = ismember(chan, botname);

  if (!m) {
    dprintf(idx, "Cannot say to %s: I'm not on that channel.\n", chan->dname);
    return;
  }

  get_user_flagrec(dcc[idx].user, &user, chan->dname);

  if ((chan->channel.mode & CHANMODER) && !me_op(chan) && !me_voice(chan)) {
    dprintf(idx, "Cannot say to %s: It is moderated.\n", chan->dname);
    return;
  }
  putlog(LOG_CMDS, "*", "#%s# (%s) say %s", dcc[idx].nick, chan->dname, par);
  privmsg(chan->name, par, DP_HELP);
  dprintf(idx, "Said to %s: %s\n", chan->dname, par);
}

static void cmd_swhois(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: swhois [server/nick] nick\n");
    return;
  }


  putlog(LOG_CMDS, "*", "#%s# swhois %s", dcc[idx].nick, par);
  if (!server_online) {
    dprintf(idx, "I am currently not connected!\n");
    return;
  }
  char *server = newsplit(&par), *nick = NULL;

  if (par[0])
    nick = newsplit(&par);

  strlcpy(dcc[idx].whois, nick ? nick : server, UHOSTLEN);
  dprintf(DP_SERVER, "WHOIS %s %s\n", server, nick ? nick : "");
}

static void cmd_kickban(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: kickban [channel|*] [-|@]<nick> [reason]\n");
    return;
  }

  char *chname = NULL;
  bool all = 0;

  if (strchr(CHANMETA, par[0]))
    chname = newsplit(&par);
  else
    chname = NULL;
  struct chanset_t* chan = get_channel(idx, chname, 1, &all);
  if (!all && !chan)
    return;

  putlog(LOG_CMDS, "*", "#%s# (%s) kickban %s", dcc[idx].nick, all ? "*" : chan->dname, par);

  char *nick = newsplit(&par), bantype = 0;

  if ((nick[0] == '@') || (nick[0] == '-')) {
    bantype = nick[0];
    nick++;
  }
  if (match_my_nick(nick)) {
    dprintf(idx, "I'm not going to kickban myself.\n");
    return;
  }

  memberlist *m = NULL;
  char *s1 = NULL, s[UHOSTLEN] = "";
  struct userrec *u = NULL;

  if (all)
    chan = chanset;
  while (chan) {

    get_user_flagrec(dcc[idx].user, &user, chan->dname);

    if (privchan(user, chan, PRIV_OP)) {
      if (all) goto next;
      dprintf(idx, "No such channel.\n");
      return;
    }
    else if (!real_chk_op(user, chan, 0)) {
      if (all) goto next;
      dprintf(idx, "You don't have access to %s\n", chan->dname);
      return;
    }
    if (!channel_active(chan)) {
      if (all) goto next;
      dprintf(idx, "I'm not on %s right now!\n", chan->dname);
      return;
    }
    if (!me_op(chan)) {
      if (all) goto next;
      dprintf(idx, "I can't help you now because I'm not a channel op"
             " on %s.\n", chan->dname);
      return;
    }

    m = ismember(chan, nick);
    if (!m) {
      if (all) goto next;
      dprintf(idx, "%s is not on %s\n", nick, chan->dname);
      return;
    }
    member_getuser(m);
    u = m->user;
    strlcpy(s, m->from, sizeof(s));
    get_user_flagrec(u, &victim, chan->dname);
  
    if ((chan_master(victim) || glob_master(victim)) &&
        !(glob_owner(user) || chan_owner(user))) {
      if (all) goto next;
      dprintf(idx, "%s is a %s master.\n", nick, chan->dname);
      return;
    }
    if (glob_bot(victim)) {
      if (all) goto next;
      dprintf(idx, "%s is another channel bot!\n", nick);
      return;
    }
    if (u_match_mask(global_exempts,s) || u_match_mask(chan->exempts, s)) {
      dprintf(idx, "%s is permanently exempted!\n", nick);
      return;
    }
    if (chan_hasop(m))
      add_mode(chan, '-', 'o', m);
    check_exemptlist(chan, s);
    switch (bantype) {
      case '@':
        s1 = strchr(s, '@');
        s1 -= 3;
        s1[0] = '*';
        s1[1] = '!';
        s1[2] = '*';
        break;
      case '-':
        s1 = strchr(s, '!');
        s1[1] = '*';
        s1--;
        s1[0] = '*';
        break;
      default:
        s1 = quickban(chan, m->userhost);
        break;
    }
    if (bantype == '@' || bantype == '-')
      do_mask(chan, chan->channel.ban, s1, 'b');
    if (!par[0])
      par = "requested";
    dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, m->nick, bankickprefix, par);
    m->flags |= SENTKICK;
    u_addmask('b', chan, s1, dcc[idx].nick, par, now + (60 * chan->ban_time), 0);
    dprintf(idx, "Kick-banned %s on %s.\n", nick, chan->dname);
    next:;
    if (!all)
      chan = NULL;
    else
      chan = chan->next;
  }
}

static void cmd_voice(int idx, char *par)
{
  char *nick = newsplit(&par), *chname = newsplit(&par);
  if (strchr(CHANMETA, nick[0]) || (!chname[0] && !strcmp(nick, "*")))
    swap(nick, chname);
  bool all = 0;

  struct chanset_t* chan = get_channel(idx, chname, 1, &all);
  if (!all && !chan)
    return;

  memberlist *m = NULL;

  if (all)
    chan = chanset;
  putlog(LOG_CMDS, "*", "#%s# (%s) voice %s", dcc[idx].nick, all ? "*" : chan->dname , nick);
  while (chan) {
    if (!nick[0] && !(nick = getnick(dcc[idx].nick, chan))[0]) {
      if (all) goto next;
      dprintf(idx, "Usage: voice <nick> [channel|*]\n");
      return;
    }
    get_user_flagrec(dcc[idx].user, &user, chan->dname);

    if (privchan(user, chan, PRIV_VOICE)) {
      if (all) goto next;
      dprintf(idx, "No such channel.\n");
      return;
    }
    else if (!chk_voice(NULL, user, chan) && !chk_op(user, chan)) {
      if (all) goto next;
      dprintf(idx, "You don't have access to voice on %s\n", chan->dname);
      return;
    }

    if (!channel_active(chan)) {
      if (all) goto next;
      dprintf(idx, "I'm not on %s right now!\n", chan->dname);
      return;
    }
    if (!me_op(chan)) {
      if (all) goto next;

      dprintf(idx, "I can't help you now because I'm not a chan op on %s.\n", 
          chan->dname);
      return;
    }

    m = ismember(chan, nick);
    if (!m) {
      if (all) goto next;
      dprintf(idx, "%s is not on %s.\n", nick, chan->dname);
      return;
    }
    add_mode(chan, '+', 'v', m);
    dprintf(idx, "Gave voice to %s on %s\n", nick, chan->dname);
    next:;
    if (!all)
      chan = NULL;
    else 
      chan = chan->next;
  }
}

static void cmd_devoice(int idx, char *par)
{
  char *nick = newsplit(&par), *chname = newsplit(&par);
  if (strchr(CHANMETA, nick[0]) || (!chname[0] && !strcmp(nick, "*")))
    swap(nick, chname);
  bool all = 0;

  struct chanset_t* chan = get_channel(idx, chname, 1, &all);
  if (!all && !chan)
    return;

  memberlist *m = NULL;

  if (all)
    chan = chanset;
  putlog(LOG_CMDS, "*", "#%s# (%s) devoice %s", dcc[idx].nick, all ? "*" : chan->dname, nick);
  while (chan) {
  if (!nick[0] && !(nick = getnick(dcc[idx].nick, chan))[0]) {
    if (all) goto next;
    dprintf(idx, "Usage: devoice <nick> [channel|*]\n");
    return;
  }
  get_user_flagrec(dcc[idx].user, &user, chan->dname);

  if (privchan(user, chan, PRIV_VOICE)) {
    if (all) goto next;
    dprintf(idx, "No such channel.\n");
    return;
  }
  else if (!chk_voice(NULL, user, chan) && !chk_op(user, chan)) {
    if (all) goto next;
    dprintf(idx, "You don't have access to devoice on %s\n", chan->dname);
    return;
  }

  if (!channel_active(chan)) {
    if (all) goto next;
    dprintf(idx, "I'm not on %s right now!\n", chan->dname);
    return;
  }
  if (!me_op(chan)) {
    if (all) goto next;
    dprintf(idx, "I can't do that right now I'm not a chan op on %s",
	    chan->dname);
    return;
  }
  m = ismember(chan, nick);
  if (!m) {
    if (all) goto next;
    dprintf(idx, "%s is not on %s.\n", nick, chan->dname);
    return;
  }

  add_mode(chan, '-', 'v', m);
  dprintf(idx, "Devoiced %s on %s\n", nick, chan->dname);
  next:;
  if (!all)
    chan = NULL;
  else
    chan = chan->next;
  }

}

static void cmd_release(int idx, char *par) {
  putlog(LOG_CMDS, "*", "#%s# release", dcc[idx].nick);
  release_nick();
}

static void cmd_op(int idx, char *par)
{
  char *nick = newsplit(&par), *chname = newsplit(&par);
  if (strchr(CHANMETA, nick[0]) || (!chname[0] && !strcmp(nick, "*")))
    swap(nick, chname);
  bool all = 0;

  struct chanset_t* chan = get_channel(idx, chname, 1, &all);
  if (!all && !chan)
    return;

  memberlist *m = NULL;
  struct userrec *u = NULL;

  if (all)
    chan = chanset;
  putlog(LOG_CMDS, "*", "#%s# (%s) op %s", dcc[idx].nick, all ? "*" : chan->dname, nick);

  while (chan) {
  get_user_flagrec(dcc[idx].user, &user, chan->dname);
  if (!nick[0] && !(nick = getnick(dcc[idx].nick, chan))[0]) {
    if (all) goto next;
    dprintf(idx, "Usage: op <nick> [channel|*]\n");
    return;
  }

  if (privchan(user, chan, PRIV_OP)) {
    if (!all)
      dprintf(idx, "No such channel.\n");
    goto next;
  }
  else if (!chk_op(user, chan)) {
    if (all) goto next;
    dprintf(idx, "You don't have access to op on %s\n", chan->dname);
    return;
  }

  if (!channel_active(chan)) {
    if (all) goto next;
    dprintf(idx, "I'm not on %s right now!\n", chan->dname);
    return;
  }
  if (!me_op(chan)) {
    if (all) goto next;
    dprintf(idx, "I can't help you now because I'm not a chan op on %s.\n",
	    chan->dname);
    return;
  }
  m = ismember(chan, nick);
  if (!m) {
    if (all) goto next;
    dprintf(idx, "%s is not on %s.\n", nick, chan->dname);
    return;
  }
  member_getuser(m);
  u = m->user;
  get_user_flagrec(u, &victim, chan->dname);
  if (chk_deop(victim, chan)) {
    dprintf(idx, "%s is currently being auto-deopped  on %s.\n", m->nick, chan->dname);
    if (all) goto next;
    return;
  }
  if (chan_bitch(chan) && !chk_op(victim, chan)) {
    dprintf(idx, "%s is not a registered op on %s.\n", m->nick, chan->dname);
    if (all) goto next;
    return;
  }

  if (do_op(m, chan, 0, 1)) {
    dprintf(idx, "Gave op to %s on %s.\n", nick, chan->dname);
    stats_add(u, 0, 1);
  }
  next:;
    if (!all)
      chan = NULL;
    else
      chan = chan->next;
  }

}

static void mass_mode(const char* chname, const char* mode, char *par)
{
  char list[101] = "", buf[2048] = "";
  size_t list_len = 0, buf_len = 0, cnt = 0;
  const unsigned short max_lines = floodless ? 15 : default_alines;
  unsigned short lines = 0;

  while (par[0]) {
    *(buf + buf_len++) = 'M';
    *(buf + buf_len++) = 'O';
    *(buf + buf_len++) = 'D';
    *(buf + buf_len++) = 'E';
    *(buf + buf_len++) = ' ';
    buf_len += strlcpy(buf + buf_len, chname, sizeof(buf) - buf_len);
    *(buf + buf_len++) = ' ';

    while ((cnt < modesperline) && par[0]) {
      /* add +mode into irc cmd */
      buf_len += strlcpy(buf + buf_len, mode, sizeof(buf) - buf_len);

      /* Make list of nicks */
      const char* nick = newsplit(&par);
      list_len += strlcpy(list + list_len, nick, sizeof(list) - list_len);
      if ((++cnt < modesperline) && par[0])
        *(list + list_len++) = ' ';
    }

    /* buf is so far: 'MODE #chan +o+o+o+o' */
    *(buf + buf_len++) = ' ';
    buf_len += strlcpy(buf + buf_len, list, sizeof(buf) - buf_len);
    *(buf + buf_len++) = '\r';
    *(buf + buf_len++) = '\n';

    if (++lines >= max_lines) {
      buf[buf_len] = 0;
      tputs(serv, buf, buf_len);
      buf[0] = buf_len = lines = 0;
    }
    list[0] = list_len = cnt = 0;
  }
  if (buf[0]) {
    buf[buf_len] = 0;
    tputs(serv, buf, buf_len);
  }
}

void mass_request(char *botnick, char *code, char *par)
{
  char* mode = newsplit(&par);

  if (strlen(mode) > 2 || !strchr("+-kb", mode[0]))
    return;

  char* chname = newsplit(&par);

  if (strchr("+-", mode[0]))
    mass_mode(chname, mode, par);
}

static void cmd_mmode(int idx, char *par)
{
  char *mode = newsplit(&par);
  if (strlen(mode) > 2 || !strchr("+-", mode[0]) || !par[0]) {
    dprintf(idx, "Usage: mmode <(+|-)MODE> <#channel> <a|o|v|d|r> [bots=n] [alines=n] [slines=n] [overlap=n] [bitch] [botbitch] [simul] [local]\n");
    dprintf(idx, "Ie. mmode -o #chan a\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# mmode %s %s", dcc[idx].nick, mode, par);

  char *chname = NULL;

  if (strchr(CHANMETA, par[0]) != NULL)
    chname = newsplit(&par);
  else
    chname = NULL;

  struct chanset_t *chan = get_channel(idx, chname);
 
  if (chan) {
    if (!shouldjoin(chan) || !channel_active(chan))  {
      dprintf(idx, "I'm not on %s\n", chan->dname);
      return;
    }
    if (channel_pending(chan)) {
      dprintf(idx, "Channel pending.\n");
      return;
    }

    get_user_flagrec(dcc[idx].user, &user, chan->dname, chan);
    if (!glob_owner(user) && !chan_owner(user)) {
      dprintf(idx, "You do not have mass mode access for %s\n", chan->dname);
      return;
    }
  }
  if (!chan || !chname || !chname[0]) {
    dprintf(idx, "Usage: mmode <(+|-)MODE> <#channel> <a|o|v|d|r> [bots=n] [alines=n] [slines=n] [overlap=n] [bitch] [botbitch] [simul] [local]\n");
    dprintf(idx, "Ie. mmode -o #chan a\n");
    return;
  }

  char *who = newsplit(&par);

  if (strlen(who) > 1 || !strchr("aovdr", who[0])) {
    dprintf(idx, "Usage: mmode <(+|-)MODE> <#channel> <a|o|v|d|r> [bots=n] [alines=n] [slines=n] [overlap=n] [bitch] [botbitch] [simul] [local]\n");
    dprintf(idx, "Ie. mmode -o #chan a\n");
    dprintf(idx, "a = all.\n");
    dprintf(idx, "o = ops.\n");
    dprintf(idx, "O = user-op.\n");
    dprintf(idx, "v = voices.\n");
    dprintf(idx, "d = non-ops.\n");
    dprintf(idx, "r = regulars (-ov).\n");
    return;
  }

  if (mode[0] == '+' && mode[1] == 'o' && !channel_fastop(chan) && !cookies_disabled) {
    dprintf(idx, STR("Error: This channel is currently set -fastop.\n"));
    dprintf(idx, STR("Mass opping would result in missing op cookies.\n"));
    dprintf(idx, STR("Please chanset the channel +fastop first.\n"));
    return;
  }


  memberlist** targets = (memberlist**) calloc(1, chan->channel.members * sizeof(memberlist *));
  int* overlaps = (int *) calloc(1, chan->channel.members * sizeof(int *));
  memberlist** chanbots = (memberlist **) calloc(1, chan->channel.members * sizeof(memberlist *));
  memberlist *m = NULL;
  int chanbotcount = 0, targetcount = 0;

  for (m = chan->channel.member; m; m = m->next) {
    if (m->user) {
      if ((m->flags & CHANOP) && m->user->bot &&
          (!m->is_me) && (nextbot(m->user->handle) >= 0)) {
        chanbots[chanbotcount++] = m;
        continue;
      } else if (m->is_me)
        continue;

      get_user_flagrec(m->user, &user, chan->dname);
    }

    bool is_target = 0;

    if (who[0] == 'o' && (m->flags & CHANOP))
      is_target = 1;
    else if (who[0] == 'O' && chk_op(user, chan))
      is_target = 1;
    else if (who[0] == 'd' && !(m->flags & CHANOP))
      is_target = 1;
    else if (who[0] == 'v' && !(m->flags & CHANOP) && (m->flags & CHANVOICE))
      is_target = 1;
    else if (who[0] == 'r' && !(m->flags & CHANOP) && !(m->flags & CHANVOICE))
      is_target = 1;
    else if (who[0] == 'a')
      is_target = 1;

    if (is_target)
      targets[targetcount++] = m;
  }

  int force_bots = 0, force_alines = 0, force_slines = 0, force_overlap = 0;
  bool bitch = 0, botbitch = 0, simul = 0, local = 0;

  while (par && par[0]) {
    char *p = newsplit(&par);
    if (!strncasecmp(p, "bots=", 5)) {
      p += 5;
      force_bots = atoi(p);
      if ((force_bots < 1) || (force_bots > chanbotcount)) {
	dprintf(idx, "bots must be within 1-%i\n", chanbotcount);
	free(targets);
	free(overlaps);
	free(chanbots);
	return;
      }
    } else if (!strncasecmp(p, "alines=", 7)) {
      p += 7;
      force_alines = atoi(p);
      if (force_alines > 5)
        dprintf(idx, "alines >5 will fail without a floodless.\n");

      if ((force_alines < 1) || (force_alines > 100)) {
	dprintf(idx, "alines must be within 1-100\n");
	free(targets);
	free(overlaps);
	free(chanbots);
	return;
      }
    } else if (!strncasecmp(p, "slines=", 7)) {
      p += 7;
      force_slines = atoi(p);
      if ((force_slines < 1) || (force_slines > 20)) {
	dprintf(idx, "slines must be within 1-20\n");
        dprintf(idx, ">6 will fail without a floodless.\n");
	free(targets);
	free(overlaps);
	free(chanbots);
	return;
      }
    } else if (!strncasecmp(p, "overlap=", 8)) {
      p += 8;
      force_overlap = atoi(p);
      if ((force_overlap < 1) || (force_overlap > 8)) {
	dprintf(idx, "overlap must be within 1-8\n");
	free(targets);
	free(overlaps);
	free(chanbots);
	return;
      }
    } else if (!strncasecmp(p, "bitch", 5)) {
      bitch = 1;
    } else if (!strncasecmp(p, "botbitch", 8)) {
      botbitch = 1;
    } else if (!strncasecmp(p, "simul", 5)) {
      simul = 1;
    } else if (!strncasecmp(p, "local", 5)) {
      local = 1;
    } else {
      dprintf(idx, "Unrecognized option %s\n", p);
      free(targets);
      free(overlaps);
      free(chanbots);
      return;
    }
  }

  if (!targetcount) {
    dprintf(idx, "No targets found on %s\n", chan->name);
    free(targets);
    free(overlaps);
    free(chanbots);
    return;
  }

  if ((!chanbotcount && !local) || (local && !me_op(chan))) {
    dprintf(idx, "No bots opped on %s\n", chan->name);
    free(targets);
    free(overlaps);
    free(chanbots);
    return;
  }

  if (local)
    force_bots = 1;

  if (local && floodless)
    force_alines = force_slines = 100;

  int overlap = (force_overlap ? force_overlap : 1);
  int needed_modes = (overlap * targetcount);
  int my_max_modes = ((force_bots ? force_bots : chanbotcount) * (force_alines ? force_alines : default_alines) * modesperline);

  if (needed_modes > my_max_modes) {
    dprintf(idx, "Need to make %d modes, but the max is %d.\n", needed_modes, my_max_modes);
    dprintf(idx, "Try increasing [alines] or decreasing [overlap].\n");
    if (overlap == 1)
      dprintf(idx, "Not enough bots.\n");
    else
      dprintf(idx, "Not enough bots. Try with overlap=1\n");
    free(targets);
    free(overlaps);
    free(chanbots);
    return;
  }


  double bots = 0;
  int amodes = 0;

  /* ok it's possible... now let's figure out how */
  if (force_bots && force_alines) {
    /* not much choice... overlap should not autochange */
    bots = force_bots;
    amodes = force_alines * modesperline;
  } else {
    if (force_bots) {
      /* calc needed modes per bot */
      bots = force_bots;
      amodes = (needed_modes + ((int)bots - 1)) / (int)bots;
    } else if (force_alines) {
      amodes = force_alines * modesperline;
      bots = (double) needed_modes / amodes;
      if (bots > (int)bots) bots = (int)bots+1; /* ceil(bots) */
    } else {
      /* Try minimizing alines, 1,2,3,4,5 against bots needed */
      amodes = 1 * modesperline;
      bots = (double) needed_modes / amodes;
      if (bots > (int)bots) bots = (int)bots+1; /* ceil(bots) */

      for (int min_alines = 2; min_alines <= 5; ++min_alines) {
        if ((int)bots > chanbotcount) {
          amodes = min_alines * modesperline;
          bots = (double) needed_modes / amodes;
          if (bots > (int)bots) bots = (int)bots+1; /* ceil(bots) */
        } else
          break;
      }

      force_slines = (amodes / modesperline);

      /* lol einride is dumb */
      if ((int)bots > chanbotcount) {
        putlog(LOG_MISC, "*", "Totally fucked calculations in cmd_mmode. this CAN'T happen.");
        dprintf(idx, "Something's wrong... bug the coder\n");
        free(targets);
        free(overlaps);
        free(chanbots);
        return;
      }
    }
  }

  /* How many modes to send */
  int smodes;

  if (force_slines)
    smodes = force_slines * modesperline;
  else
    smodes = default_alines * modesperline;
  if (smodes < amodes)
    smodes = amodes;

  dprintf(idx, "Mass %s of %s\n", mode, chan->dname);
  dprintf(idx, "  %d bots used.\n", (int)bots);
  dprintf(idx, "  %d targets.\n", targetcount);
  dprintf(idx, "  %d assumed modes per participating bot.\n", amodes);
  dprintf(idx, "  %d assumed mode lines per participating bot.\n", amodes / modesperline);
  dprintf(idx, "  %d max modes per participating bot.\n", smodes);
  dprintf(idx, "  %d max mode lines per participating bot.\n", smodes / modesperline);
  dprintf(idx, "  %d assumed modes per target nick.\n", overlap);

  int tpos = 0, bpos = 0, i = 0;
  char **work_list = (char**) calloc((int)bots, sizeof(char**));
  char *work = NULL;
  size_t work_size = 2048;

  /* now use bots/modes to distribute nicks to MODE on */
  while (bots) {
    work = work_list[bpos] = (char*) calloc(1, work_size);

    if (local) {
      strlcpy(work, mode, work_size);
      strlcat(work, " ", work_size);
      strlcat(work, chan->dname, work_size);
    } else if (!simul)
      simple_snprintf(work, work_size, "mr %s %s", mode, chan->dname);
    else
      work[0] = 0;

    /* Make list of assumed lines (alines) */
    for (i = 0; i < amodes; ++i) {
      if (overlaps[tpos]++ < overlap) {
        strlcat(work, " ", work_size);
        strlcat(work, targets[tpos]->nick, work_size);
      }
      if (++tpos >= targetcount)
        tpos = 0;
    }

    /* Now make lines of throttled modes (send lines) (slines) */
    if (smodes > amodes) {
      int atpos = tpos;
      for (i = 0; i < (smodes - amodes); ++i) {
        if (overlaps[atpos]++ < overlap) {
          strlcat(work, " ", work_size);
          strlcat(work, targets[atpos]->nick, work_size);
        }
	if (++atpos >= targetcount)
          atpos = 0;
      }
    }
    --bots;
    ++bpos;
  }

  bots = bpos;
  bpos = 0;

  /* QUEUE *ALL*, then dump */

  while (bots) {
    work = work_list[bpos];

    if (simul)
      dprintf(idx, "%-10s MODE %s %s\n", local ? botname : chanbots[bpos]->nick, mode, work);
    else if (local)
      mass_request(conf.bot->nick, "mr", work);
    else
      putbot(chanbots[bpos]->user->handle, work);

    free(work_list[bpos]);
    --bots;
    ++bpos;
  }
  free(work_list);

  if (bitch && !simul) {
    chan->status |= CHAN_BITCH;
    do_chanset(NULL, chan, "+bitch", DO_LOCAL | DO_NET);
  }
  if (botbitch && !simul) {
    chan->status |= CHAN_BOTBITCH;
    do_chanset(NULL, chan, "+botbitch", DO_LOCAL | DO_NET);
  }
  free(targets);
  free(overlaps);
  free(chanbots);
  return;
}

static void cmd_deop(int idx, char *par)
{
  char *nick = newsplit(&par), *chname = newsplit(&par);
  if (strchr(CHANMETA, nick[0]) || (!chname[0] && !strcmp(nick, "*")))
    swap(nick, chname);
  bool all = 0;

  struct chanset_t* chan = get_channel(idx, chname, 1, &all);
  if (!all && !chan)
    return;

  memberlist *m = NULL;
  struct userrec *u = NULL;

  if (all)
    chan = chanset;
  putlog(LOG_CMDS, "*", "#%s# (%s) deop %s", dcc[idx].nick, all ? "*" : chan->dname, nick);

  while (chan) {
    get_user_flagrec(dcc[idx].user, &user, chan->dname);
    if (!nick[0] && !(nick = getnick(dcc[idx].nick, chan))[0]) {
      if (all) goto next;  
      dprintf(idx, "Usage: deop <nick> [channel|*]\n");
      return;
    }
    if (privchan(user, chan, PRIV_OP)) {
      if (all) goto next;
      dprintf(idx, "No such channel.\n");
    }
    else if (!real_chk_op(user, chan, 0)) {
      if (all) goto next;
      dprintf(idx, "You don't have access to deop on %s\n", chan->dname);
      return;
    }
    if (!channel_active(chan)) {
      if (all) goto next;  
      dprintf(idx, "I'm not on %s right now!\n", chan->dname);
      return;
    }
    if (!me_op(chan)) {
      if (all) goto next;  
      dprintf(idx, "I can't help you now because I'm not a chan op on %s.\n",
  	    chan->dname);
      return;
    }
    m = ismember(chan, nick);
    if (!m) {
      if (all) goto next;  
      dprintf(idx, "%s is not on %s.\n", nick, chan->dname);
      return;
    }
    if (match_my_nick(nick)) {
      if (all) goto next;  
      dprintf(idx, "I'm not going to deop myself.\n");
      return;
    }
    member_getuser(m);
    u = m->user;
    get_user_flagrec(u, &victim, chan->dname);

    if ((chan_master(victim) || glob_master(victim)) &&
        !(chan_owner(user) || glob_owner(user))) {
      dprintf(idx, "%s is a master for %s.\n", m->nick, chan->dname);
      if (all) goto next;  
      return;
    }
    if (glob_bot(victim)) {
      dprintf(idx, "%s is another channel bot!\n", nick);
      return;
    }
    if (chk_op(victim, chan) && !(chan_master(user) || glob_master(user))) {
      dprintf(idx, "%s has the op flag for %s.\n", m->nick, chan->dname);
      if (all) goto next;  
      return;
    }
    add_mode(chan, '-', 'o', m);
    dprintf(idx, "Deopped %s on %s.\n", nick, chan->dname);
    next:;
    if (!all)
      chan = NULL;
    else
      chan = chan->next;
  }
}

static void cmd_kick(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: kick [channel|*] <nick> [reason]\n");
    return;
  }

  char *chname = NULL;
  bool all = 0;

  if (strchr(CHANMETA, par[0]))
    chname = newsplit(&par);
  else
    chname = NULL;
  struct chanset_t* chan = get_channel(idx, chname, 1, &all);
  if (!all && !chan)
    return;

  putlog(LOG_CMDS, "*", "#%s# (%s) kick %s", dcc[idx].nick, all ? "*" : chan->dname, par);

  char *nick = newsplit(&par);

  if (!par[0])
    par = "requested";
  if (match_my_nick(nick)) {
    dprintf(idx, "I'm not going to kick myself.\n");
    return;
  }

  memberlist *m = NULL;
  struct userrec *u = NULL;

  if (all)
    chan = chanset;
  while (chan) {
    get_user_flagrec(dcc[idx].user, &user, chan->dname);

    if (privchan(user, chan, PRIV_OP)) {
      if (all) goto next;
      dprintf(idx, "No such channel.\n");
      return;
    }
    else if (!real_chk_op(user, chan, 0)) {
      if (all) goto next;
      dprintf(idx, "You don't have access to kick on %s\n", chan->dname);
      return;
    }

    if (!channel_active(chan)) {
      if (all) goto next;
      dprintf(idx, "I'm not on %s right now!\n", chan->dname);
      return;
    }
    if (!me_op(chan)) {
      if (all) goto next;
      dprintf(idx, "I can't help you now because I'm not a channel op on %s.\n", chan->dname);
      return;
    }

    m = ismember(chan, nick);
    if (!m) {
      if (all) goto next;
      dprintf(idx, "%s is not on %s\n", nick, chan->dname);
      return;
    }
    member_getuser(m);
    u = m->user;
    get_user_flagrec(u, &victim, chan->dname);
    if (chk_op(victim, chan) && !(chan_master(user) || glob_master(user))) {
      if (all) goto next;
      dprintf(idx, "%s is a legal op.\n", nick);
      return;
    }
    if ((chan_master(victim) || glob_master(victim)) &&
        !(glob_owner(user) || chan_owner(user))) {
      if (all) goto next;
      dprintf(idx, "%s is a %s master.\n", nick, chan->dname);
      return;
    }
    if (glob_bot(victim)) {
      dprintf(idx, "%s is another channel bot!\n", nick);
      return;
    }
    dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, par);
    m->flags |= SENTKICK;
    dprintf(idx, "Kicked %s on %s.\n", nick, chan->dname);
    next:;
    if (!all)
      chan = NULL;
    else
      chan = chan->next;
  }
}

static void cmd_getkey(int idx, char *par)
{
  struct chanset_t *chan = NULL;

  chan = get_channel(idx, par);
  if (!chan || !has_op(idx, chan))
    return;

  putlog(LOG_CMDS, "*", "#%s getkey %s", dcc[idx].nick, par);

  get_user_flagrec(dcc[idx].user, &user, chan->dname);

  if (privchan(user, chan, PRIV_OP)) {
    dprintf(idx, "No such channel.\n");
    return;
  }
  else if (!real_chk_op(user, chan, 0)) {
    dprintf(idx, "You don't have access for %s\n", chan->dname);
    return;
  }

  if (!(channel_pending(chan) || channel_active(chan))) {
    dprintf(idx, "I'm not on %s right now.\n", chan->dname);
    return;
  }

  char outbuf[201] = "";

  if (!chan->channel.key[0])
    simple_snprintf(outbuf, sizeof(outbuf), "%s has no key set.", chan->dname);
  else
    simple_snprintf(outbuf, sizeof(outbuf), "Key for %s is: %s", chan->dname, chan->channel.key);
  if (chan->key_prot[0]) {
    size_t olen = strlen(outbuf);
    simple_snprintf(&outbuf[olen], sizeof(outbuf) - olen, " (Enforcing +k %s)", chan->key_prot);
  }
  dprintf(idx, "%s\n", outbuf);
}

static void cmd_mop(int idx, char *par)
{
  bool found = 0, all = 0;
  char *chname = newsplit(&par);

  struct chanset_t* chan = get_channel(idx, chname, 1, &all);

  if (all) {
    get_user_flagrec(dcc[idx].user, &user, NULL);
    if (!glob_owner(user)) {
      dprintf(idx, "You do not have access to mop '*'\n");
      return;
    }
    chan = chanset;
  }

  if (!chan && !all) {
    dprintf(idx, "Usage: mop <channel|*>\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# (%s) mop %s", dcc[idx].nick, all ? "*" : chan->dname, par);

  memberlist *m = NULL;

  while (chan) {
    get_user_flagrec(dcc[idx].user, &user, chan->dname);
    if (privchan(user, chan, PRIV_OP)) {
      if (all) goto next;
      dprintf(idx, "No such channel.\n");
      return;
    }
    if (!chk_op(user, chan)) {
      if (all) goto next;
      dprintf(idx, "You are not a channel op on %s.\n", chan->dname);
      return;
    }
    if (!me_op(chan)) {
      if (all) goto next;
      dprintf(idx, "I am not opped on %s.\n", chan->dname);
      return;
    }
    if (channel_active(chan) && !channel_pending(chan)) {
      for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
        member_getuser(m);
        if (m->user && u_pass_match(m->user, "-"))
          continue;		/* dont op users without a pass */
        get_user_flagrec(m->user, &victim, chan->dname);
        if (!chan_hasop(m) && !glob_bot(victim) && chk_op(victim, chan)) {
          found = 1;
          dprintf(idx, "Gave op to '%s' as '%s' on %s\n", m->user->handle, m->nick, chan->dname);
          do_op(m, chan, 0, 0);
        }
      }
    } else {
      if (!all)
        dprintf(idx, "Channel %s is not active or is pending.\n", chan->dname);
      return;
    }
    if (!found && !all)
      dprintf(idx, "No one to op on %s\n", chan->dname);
    next:;
    if (!all)
      chan = NULL;
    else
      chan = chan->next;
  }
}


static void cmd_find(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: find <nick!ident@host.com>|<user> (wildcard * allowed)\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# find %s", dcc[idx].nick, par);

  struct chanset_t *chan = NULL, **cfound = NULL;
  memberlist *m = NULL, **found = NULL;
  int fcount = 0;
  bool tr = 0, lookup_user = 0;
  struct userrec *u = NULL;

  if (!strchr(par, '!')) {
    lookup_user = 1;
    u = get_user_by_handle(userlist, par);
    if (!u) {
      dprintf(idx, "No such user: %s\n", par);
      return;
    }
  }    
  /* make a list of members in found[] */
  for (chan = chanset; chan; chan = chan->next) {

    get_user_flagrec(dcc[idx].user, &user, chan->dname);

    if (!privchan(user, chan, PRIV_OP)) {

      for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
        member_getuser(m, 1);

        if ((!lookup_user && wild_match(par, m->from)) || (lookup_user && m->user == u)) {
          fcount++;
          if (!found) {
            found = (memberlist **) calloc(1, sizeof(memberlist *) * 100);
            cfound = (struct chanset_t **) calloc(1, sizeof(struct chanset_t *) * 100);
          }
          found[fcount - 1] = m;
          cfound[fcount - 1] = chan;
          if (fcount == 100) {
            tr = 1;
            break;
          }
        }
      }
    }
    if (fcount == 100) {
      tr = 1;
      break;
    }
  }

  if (fcount) {
    char tmp[1024] = "";
    int findex, i;

    for (findex = 0; findex < fcount; findex++) {
      if (found[findex]) {
        simple_snprintf(tmp, sizeof(tmp), "%s!%s %s%s%s on %s", found[findex]->nick, found[findex]->userhost,
         found[findex]->user ? "(user:" : "", found[findex]->user ? found[findex]->user->handle : "", found[findex]->user ? ")" : "", 
                                           cfound[findex]->name);
        for (i = findex + 1; i < fcount; i++) {
          if (found[i] && (!strcmp(found[i]->nick, found[findex]->nick))) {
            strlcat(tmp, ", ", sizeof(tmp));
            strlcat(tmp, cfound[i]->name, sizeof(tmp));
            found[i] = NULL;
          }
        }
        dprintf(idx, "%s\n", tmp);
      }
    }
    free(found);
    free(cfound);
  } else {
    dprintf(idx, "No matches for %s on any channels.\n", par);
  }
  if (tr)
    dprintf(idx, "(more than 100 matches; list truncated)\n");
  dprintf(idx, "--- Found %d matches.\n", fcount);
}

static void do_invite(int idx, char *par, bool op)
{
  char *nick = newsplit(&par), *chname = newsplit(&par);

  if (!nick[0]) {
    dprintf(idx, "Usage: invite <nickname> [channel|*]\n");
    return;
  }

  if (strchr(CHANMETA, nick[0]) || (!chname[0] && !strcmp(nick, "*")))
    swap(nick, chname);
  bool all = 0;

  struct chanset_t* chan = get_channel(idx, chname, 1, &all);
  if (!all && !chan)
    return;

  memberlist *m = NULL;

  if (all)
    chan = chanset;

  putlog(LOG_CMDS, "*", "#%s# (%s) %s %s", dcc[idx].nick, all ? "*" : chan->dname, op ? "iop" : "invite", nick);

  while (chan) {

    get_user_flagrec(dcc[idx].user, &user, chan->dname);
    if (privchan(user, chan, PRIV_OP)) {
      if (all) goto next;
      dprintf(idx, "No such channel.\n");
    }
    else if (!real_chk_op(user, chan, 0)) {
      if (all) goto next;
      dprintf(idx, "You don't have access to invite to %s\n", chan->dname);
      return;
    }

    if (!me_op(chan)) {
      if (all) goto next;
      if (chan->channel.mode & CHANINV) {
        dprintf(idx, "I can't help you now because I'm not a channel op on %s", chan->dname);
        return;
      }
      if (!channel_active(chan)) {
        dprintf(idx, "I'm not on %s right now!\n", chan->dname);
        return;
      }
    }
    m = ismember(chan, nick);
    if (m && !chan_issplit(m)) {
      if (all) goto next;
      dprintf(idx, "%s is already on %s!\n", nick, chan->dname);
      return;
    }

    cache_invite(chan, nick, NULL, NULL, op, 0);
    dprintf(idx, "Inviting %s to %s.\n", nick, chan->dname);
    next:;
    if (!all)
      chan = NULL;
    else
    chan = chan->next;
  }
}

static void cmd_invite(int idx, char *par)
{
  do_invite(idx, par, 0);
}

#ifdef CACHE
static void cmd_iop(int idx, char *par)
{
  do_invite(idx, par, 1);
}
#endif /* CACHE */

static void cmd_authed(int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# authed"), dcc[idx].nick);

  dprintf(idx, STR("Authed:\n"));
  Auth::TellAuthed(idx);
}

static void cmd_roles(int idx, char *par)
{
  struct chanset_t* chan = NULL;
  size_t roleidx;
  int role;

  chan = get_channel(idx, par);
  if (!chan) {
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# (%s) roles", dcc[idx].nick, chan->dname);

  if (!channel_active(chan)) {
    dprintf(idx, "I'm not on %s right now!\n", chan->dname);
    return;
  }

  if (chan->bot_roles->size() == 0) {
    dprintf(idx, "Roles for %s are not yet calculated.\n", chan->dname);
    return;
  }

  dprintf(idx, "Roles for %s:\n", chan->dname);

  /* Advertise roles */
  for (roleidx = 0; role_counts[roleidx].name; roleidx++) {
    role = role_counts[roleidx].role;
    dprintf(idx, "  %-8s: %s\n", role_counts[roleidx].name,
        static_cast<bd::String>((*chan->role_bots)[role].join(" ")).c_str());
  }
}

static void cmd_channel(int idx, char *par)
{
  struct chanset_t *chan = NULL;

  chan = get_channel(idx, par);
  if (!chan || !has_op(idx, chan))
    return;

  char handle[HANDLEN + 1] = "", s[UHOSTLEN] = "", s1[UHOSTLEN] = "", atrflag = 0, chanflag[2] = "";
  memberlist *m = NULL;
  size_t maxnicklen, maxhandlen;
  char format[81] = "";

  get_user_flagrec(dcc[idx].user, &user, chan->dname);

  putlog(LOG_CMDS, "*", "#%s# (%s) channel", dcc[idx].nick, chan->dname);
  strlcpy(s, getchanmode(chan), sizeof s);
  if (channel_pending(chan)) {
    simple_snprintf(s1, sizeof s1, "Processing channel %s", chan->dname);
  } else if (channel_active(chan)) {
    simple_snprintf(s1, sizeof s1, "Channel %s", chan->dname);
  } else {
    simple_snprintf(s1, sizeof s1, "Desiring channel %s", chan->dname);
  }
  dprintf(idx, "%s, %d member%s, mode %s:\n", s1, chan->channel.members,
	  chan->channel.members == 1 ? "" : "s", s);
  if (chan->channel.splitmembers)
    dprintf(idx, "%d split members\n", chan->channel.splitmembers);
  if (chan->channel.topic)
    dprintf(idx, "%s: %s\n", "Channel Topic", chan->channel.topic);
  if (channel_active(chan)) {
    /* find max nicklen and handlen */
    maxnicklen = maxhandlen = 0;
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
	if (strlen(m->nick) > maxnicklen)
	    maxnicklen = strlen(m->nick);
	if (m->user)
	    if (strlen(m->user->handle) > maxhandlen)
		maxhandlen = strlen(m->user->handle);
    }
    if (maxnicklen < 9) maxnicklen = 9;
    if (maxhandlen < 9) maxhandlen = 9;
    
    dprintf(idx, "(n = owner, m = master, o = op, d = deop, b = bot) CAP:global\n");
    simple_snprintf(format, sizeof format, " %%-%zus %%-%zus %%-6s %%-4s %%-5s %%s %%s\n", 
			maxnicklen, maxhandlen);
    dprintf(idx, format, "NICKNAME", "HANDLE", " JOIN", "  HOPS", "IDLE", "USER@HOST", "USER@IP");
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      if (m->joined > 0) {
	if ((now - (m->joined)) > 86400)
	  strftime(s, 6, "%d%b", gmtime(&(m->joined)));
	else
	  strftime(s, 6, "%H:%M", gmtime(&(m->joined)));
      } else
	strlcpy(s, " --- ", sizeof s);

      // Force +r (re)processing
      if (!m->userip[0] && doresolv(chan)) {
        char host[UHOSTLEN] = "", *p = NULL;
        p = strchr(m->userhost, '@');
        if (p) {
          ++p;
          strlcpy(host, p, strlen(m->userhost) - (p - host));
          resolve_to_member(chan, m->nick, host);
        }
      }

      member_getuser(m, 1);

      if (m->user == NULL)
	strlcpy(handle, "*", sizeof handle);
       else
       	strlcpy(handle, m->user->handle, sizeof handle);
      get_user_flagrec(m->user, &user, chan->dname);
      /* Determine status char to use */
      if (glob_bot(user) && chk_op(user, chan))
        atrflag = 'B';
      else if (glob_bot(user))
        atrflag = 'b';
      else if (glob_owner(user))
        atrflag = 'N';
      else if (chan_owner(user))
        atrflag = 'n';
      else if (glob_master(user))
        atrflag = 'M';
      else if (chan_master(user))
        atrflag = 'm';
      else if (glob_deop(user))
        atrflag = 'D';
      else if (chan_deop(user))
        atrflag = 'd';
      else if (glob_autoop(user))
        atrflag = 'A';
      else if (chan_autoop(user))
        atrflag = 'a';
      else if (glob_op(user) && !privchan(user, chan, PRIV_OP))
        atrflag = 'O';
      else if (chan_op(user))
        atrflag = 'o';
      else if (glob_quiet(user))
        atrflag = 'Q';
      else if (chan_quiet(user))
        atrflag = 'q';
      else if (glob_voice(user) && !privchan(user, chan, PRIV_VOICE))
        atrflag = 'V';
      else if (chan_voice(user))
        atrflag = 'v';
      else if (glob_kick(user))
        atrflag = 'K';
      else if (chan_kick(user))
        atrflag = 'k';
      else if (glob_wasoptest(user))
        atrflag = 'W';
      else if (chan_wasoptest(user))
        atrflag = 'w';
      else
	atrflag = ' ';

      if (chan_hasop(m))
        chanflag[1] = '@';
      else if (chan_hasvoice(m))
        chanflag[1] = '+';
      else
        chanflag[1] = ' ';
      if (m->flags & OPER)
        chanflag[0] = 'O';
      else
        chanflag[0] = ' ';

      if (chan_issplit(m)) {
        simple_snprintf(format, sizeof format, 
			"%%c%%c%%-%zus %%-%zus %%s %%c     <- netsplit, %%lus\n", 
			maxnicklen, maxhandlen);
	dprintf(idx, format, chanflag[0],chanflag[1], m->nick, handle, s, atrflag,
		now - (m->split));
      } else if (m->is_me) {
        simple_snprintf(format, sizeof format, 
			"%%c%%c%%-%zus %%-%zus %%s %%c     <- it's me!\n", 
			maxnicklen, maxhandlen);
	dprintf(idx, format, chanflag[0], chanflag[1], m->nick, handle, s, atrflag);
      } else {
	/* Determine idle time */
	if (now - (m->last) > 86400)
	  simple_snprintf(s1, sizeof s1, "%2dd", (int) ((now - (m->last)) / 86400));
	else if (now - (m->last) > 3600)
	  simple_snprintf(s1, sizeof s1, "%2dh", (int) ((now - (m->last)) / 3600));
	else if (now - (m->last) > 180)
	  simple_snprintf(s1, sizeof s1, "%2dm", (int) ((now - (m->last)) / 60));
	else
	  strlcpy(s1, "   ", sizeof s1);
	simple_snprintf(format, sizeof format, "%%c%%c%%-%zus %%-%zus %%s %%c   %%d %%s  %%s %%s\n", 
			maxnicklen, maxhandlen);
	dprintf(idx, format, chanflag[0], chanflag[1], m->nick,	handle, s, atrflag, m->hops,
                     s1, m->userhost, m->userip);
      }
      if (chan_fakeop(m))
	dprintf(idx, "    (FAKE CHANOP GIVEN BY SERVER)\n");
      if (chan_sentop(m))
	dprintf(idx, "    (pending +o -- I'm lagged)\n");
      if (chan_sentdeop(m))
	dprintf(idx, "    (pending -o -- I'm lagged)\n");
      if (chan_sentkick(m))
	dprintf(idx, "    (pending kick)\n");
    }
  }
  dprintf(idx, "End of channel info.\n");
}

static void cmd_topic(int idx, char *par)
{
  struct chanset_t *chan = NULL;

  if (par[0] && (strchr(CHANMETA, par[0]) != NULL)) {
    chan = get_channel(idx, newsplit(&par));
  } else
    chan = get_channel(idx, "");

  if (!chan || !has_op(idx, chan))
    return;

  get_user_flagrec(dcc[idx].user, &user, chan->dname);

  if (!channel_active(chan)) {
    dprintf(idx, "I'm not on %s right now!\n", chan->dname);
    return;
  }
  if (!par[0]) {
    if (chan->channel.topic) {
      dprintf(idx, "The topic for %s is: %s\n", chan->dname,
	chan->channel.topic);
    } else {
      dprintf(idx, "No topic is set for %s\n", chan->dname);
    }
  } else if (channel_optopic(chan) && !me_op(chan)) {
    dprintf(idx, "I'm not a channel op on %s and the channel %s",
	  "is +t.\n", chan->dname);
  } else {
    dprintf(DP_SERVER, "TOPIC %s :%s\n", chan->name, par);
    dprintf(idx, "Changing topic...\n");
    putlog(LOG_CMDS, "*", "#%s# (%s) topic %s", dcc[idx].nick,
	    chan->dname, par);
  }
}

static void cmd_resetbans(int idx, char *par)
{
  struct chanset_t *chan = get_channel(idx, newsplit(&par));

  if (!chan || !has_op(idx, chan))
    return;

  get_user_flagrec(dcc[idx].user, &user, chan->dname);

  putlog(LOG_CMDS, "*", "#%s# (%s) resetbans", dcc[idx].nick, chan->dname);
  dprintf(idx, "Resetting bans on %s...\n", chan->dname);
  resetbans(chan);
}

static void cmd_resetexempts(int idx, char *par)
{
  struct chanset_t *chan = get_channel(idx, newsplit(&par));

  if (!chan || !has_op(idx, chan))
    return;

  get_user_flagrec(dcc[idx].user, &user, chan->dname);

  putlog(LOG_CMDS, "*", "#%s# (%s) resetexempts", dcc[idx].nick, chan->dname);
  dprintf(idx, "Resetting exempts on %s...\n", chan->dname);
  resetexempts(chan);
}

static void cmd_resetinvites(int idx, char *par)
{
  struct chanset_t *chan = get_channel(idx, newsplit(&par));

  if (!chan || !has_op(idx, chan))
    return;
  get_user_flagrec(dcc[idx].user, &user, chan->dname);

  putlog(LOG_CMDS, "*", "#%s# (%s) resetinvites", dcc[idx].nick, chan->dname);
  dprintf(idx, "Resetting resetinvites on %s...\n", chan->dname);
  resetinvites(chan);
}

static void cmd_adduser(int idx, char *par)
{
  if ((!par[0]) || ((par[0] =='!') && (!par[1]))) {
    dprintf(idx, "Usage: adduser <nick> [handle]\n");
    return;
  }

  char *nick = NULL, *hand = NULL;
  struct chanset_t *chan = NULL;
  struct userrec *u = NULL;
  memberlist *m = NULL;
  char s[UHOSTLEN] = "", s1[UHOSTLEN] = "", s2[MAXPASSLEN + 1] = "", s3[MAXPASSLEN + 1] = "", tmp[50] = "";
  int atr = dcc[idx].user ? dcc[idx].user->flags : 0;
  bool statichost = 0;
  char *p1 = s1;

  putlog(LOG_CMDS, "*", "#%s# adduser %s", dcc[idx].nick, par);

  nick = newsplit(&par);

  /* This flag allows users to have static host (added by drummer, 20Apr99) */
  if (nick[0] == '!') {
    statichost = 1;
    nick++;
  }
  if (!par[0]) {
    hand = nick;
  } else {
    bool ok = 1;

    for (char *p = par; *p; p++)
      if ((*p <= 32) || (*p >= 127))
	ok = 0;
    if (!ok) {
      dprintf(idx, "You can't have strange characters in a nick.\n");
      return;
    } else if (strchr("-,+*=:!.@#;$", par[0]) != NULL) {
      dprintf(idx, "You can't start a nick with '%c'.\n", par[0]);
      return;
    }
    hand = par;
  }

  for (chan = chanset; chan; chan = chan->next) {
    m = ismember(chan, nick);
    if (m)
      break;
  }
  if (!m) {
    dprintf(idx, "%s is not on any channels I monitor\n", nick);
    return;
  }
  if (strlen(hand) > HANDLEN)
    hand[HANDLEN] = 0;
  member_getuser(m);
  strlcpy(s, m->from, sizeof(s));
  if ((u = m->user)) {
    dprintf(idx, "%s is already known as %s.\n", nick, u->handle);
    return;
  }
  u = get_user_by_handle(userlist, hand);
  if (u && (u->flags & (USER_OWNER | USER_MASTER)) &&
      !(atr & USER_OWNER) && strcasecmp(dcc[idx].nick, hand)) {
    dprintf(idx, "You can't add hostmasks to the bot owner/master.\n");
    return;
  }
  if (!statichost)
    maskaddr(s, s1, 0); /* *!user@host.com */
  else {
    strlcpy(s1, s, sizeof s1);
    p1 = strchr(s1, '!');
    if (strchr("~^+=-", p1[1]))
      p1[1] = '?';
    p1--;
    p1[0] = '*';
  }
  if (!u) {
    userlist = adduser(userlist, hand, p1, "-", USER_DEFAULT, 0);
    u = get_user_by_handle(userlist, hand);
    simple_snprintf(tmp, sizeof(tmp), "%li %s", (long) now, dcc[idx].nick);
    set_user(&USERENTRY_ADDED, u, tmp);
    make_rand_str(s2, MAXPASSLEN);
    set_user(&USERENTRY_PASS, u, s2);

    make_rand_str(s3, MAXPASSLEN);
    set_user(&USERENTRY_SECPASS, u, s3);

    dprintf(idx, "Added [%s]%s with no flags.\n", hand, p1);
    dprintf(idx, "%s's initial password set to \002%s\002\n", hand, s2);
    dprintf(idx, "%s's initial secpass set to \002%s\002\n", hand, s3);

    bd::String msg;
    msg = bd::String::printf("*** You've been add to this botnet as '%s' with the host '%s'. Ask a botnet admin for the msg cmds. Your initial password is: %s", hand, p1, s2);
    privmsg(nick, msg, DP_HELP);
  } else {
    dprintf(idx, "Added hostmask %s to %s.\n", p1, u->handle);
    addhost_by_handle(hand, p1);
    get_user_flagrec(u, &user, chan->dname);
    check_this_user(hand, 0, NULL);
  }

}

static void cmd_deluser(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: deluser <nick>\n");
    return;
  }

  char *nick = NULL, *added = NULL;
  struct chanset_t *chan = NULL;
  memberlist *m = NULL;
  struct userrec *u = NULL;

  nick = newsplit(&par);

  for (chan = chanset; chan; chan = chan->next) {
    m = ismember(chan, nick);
    if (m)
      break;
  }
  if (!m) {
    dprintf(idx, "%s is not on any channels I monitor\n", nick);
    return;
  }
  get_user_flagrec(dcc[idx].user, &user, chan->dname);
  member_getuser(m);
  if (!(u = m->user)) {
    dprintf(idx, "%s is not a valid user.\n", nick);
    return;
  }
  added = (char *) get_user(&USERENTRY_ADDED, u);
  newsplit(&added);

  get_user_flagrec(u, &victim, NULL);
  if (isowner(u->handle)) {
    dprintf(idx, "You can't remove a permanent bot owner!\n");
  } else if (glob_admin(victim) && !isowner(dcc[idx].nick)) {
    dprintf(idx, "You can't remove an admin!\n");
  } else if (glob_owner(victim)) {
    dprintf(idx, "You can't remove a bot owner!\n");
  } else if (chan_owner(victim) && !glob_owner(user)) {
    dprintf(idx, "You can't remove a channel owner!\n");
  } else if (chan_master(victim) && !(glob_owner(user) || chan_owner(user))) {
    dprintf(idx, "You can't remove a channel master!\n");
  } else if (glob_bot(victim) && !glob_owner(user)) {
    dprintf(idx, "You can't remove a bot!\n");
  } else if (!glob_master(user) && strcasecmp(dcc[idx].nick, added)) {
    dprintf(idx, "Sorry, you may not delete this user as you did not add them.\n");
  } else {
    char buf[HANDLEN + 1] = "";

    strlcpy(buf, u->handle, sizeof buf);
    if (deluser(u->handle)) {
      dprintf(idx, "Deleted %s.\n", buf); /* ?!?! :) */
      putlog(LOG_CMDS, "*", "#%s# deluser %s [%s]", dcc[idx].nick, nick, buf);
    } else {
      dprintf(idx, "Failed.\n");
    }
  }
}

static void cmd_reset(int idx, char *par)
{
  struct chanset_t *chan = NULL;

  if (par[0]) {
    chan = findchan_by_dname(par);

    if (chan)
      get_user_flagrec(dcc[idx].user, &user, chan->dname);
    if (!chan || privchan(user, chan, PRIV_OP)) {
      dprintf(idx, "I don't monitor that channel.\n");
    } else {
      get_user_flagrec(dcc[idx].user, &user, par);
      if (!glob_master(user) && !chan_master(user)) {
	dprintf(idx, "You are not a master on %s.\n", chan->dname);
      } else if (!channel_active(chan)) {
	dprintf(idx, "I'm not on %s at the moment!\n", chan->dname);
      } else {
	putlog(LOG_CMDS, "*", "#%s# reset %s", dcc[idx].nick, par);
	dprintf(idx, "Resetting channel info for %s...\n", chan->dname);
	reset_chan_info(chan);
      }
    }
  } else if (!(dcc[idx].user->flags & USER_MASTER)) {
    dprintf(idx, "You are not a Bot Master.\n");
  } else {
    putlog(LOG_CMDS, "*", "#%s# reset all", dcc[idx].nick);
    dprintf(idx, "Resetting channel info for all channels...\n");
    for (chan = chanset; chan; chan = chan->next) {
      if (channel_active(chan) || (!channel_active(chan) && shouldjoin(chan)))
	reset_chan_info(chan);
    }
  }
}

static void cmd_play(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: play [channel] <file>\n");
    return;
  }

  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (strchr(CHANMETA, par[0]) != NULL)
    chname = newsplit(&par);
  else
    chname = 0;
  chan = get_channel(idx, chname);
  if (!chan)
    return;

  if (!par[0]) {
    dprintf(idx, "Usage: play [channel] <file>\n");
    return;
  }

  memberlist *m = ismember(chan, botname);

  if (!m) {
    dprintf(idx, "Cannot play to %s: I'm not on that channel.\n", chan->dname);
    return;
  }

  get_user_flagrec(dcc[idx].user, &user, chan->dname);

  if (!me_op(chan) && !isowner(dcc[idx].nick)) {
    dprintf(idx, "Cannot play to %s: I am not opped.\n", chan->dname);
    dprintf(idx, "Only owners may play to a channel the bot is not opped in.\n");
    return;
  }

  if (!me_op(chan) && !me_voice(chan)) {
    dprintf(idx, "Cannot play to %s: I am not voiced or opped.\n", chan->dname);
    return;
  }
  putlog(LOG_CMDS, "*", "#%s# (%s) play %s", dcc[idx].nick, chan->dname, par);

  // Ensure file exists and is within proper path
  bd::String file(par);

  if (file[0] == '/' || file(0, 2) == "..") {
    dprintf(idx, "Cannot play '%s': Illegal path.\n", par);
    return;
  }

  if (!can_stat(par)) {
    dprintf(idx, "Cannot play '%s': Cannot access file.\n", par);
    return;
  }

  bd::Stream stream;
  stream.loadFile(par);
  bd::String str;
  size_t lines = 0;
  while (stream.tell() < stream.length()) {
    str = stream.getline().chomp();
    if (str.length()) {
      privmsg(chan->name, str, DP_PLAY);
      ++lines;
    }
  }
  dprintf(idx, "Playing %zu lines from %s to %s\n", lines, par, chan->dname);
  long time_to_play = 0;
  if (floodless)
    time_to_play = 0;
  else if (lines < 10)
    time_to_play = 3;
  else
    time_to_play = 3 + ((lines - 10) / 2);

  dprintf(idx, "Estimated time-to-play: %li seconds\n", time_to_play);
}

static cmd_t irc_dcc[] =
{
  {"act",		"o|o",	 (Function) cmd_act,		NULL, LEAF},
  {"adduser",		"m|m",	 (Function) cmd_adduser,	NULL, LEAF|AUTH},
  {"authed",		"n",	 (Function) cmd_authed,		NULL, LEAF},
  {"channel",		"o|o",	 (Function) cmd_channel,	NULL, LEAF},
  {"deluser",		"m|m",	 (Function) cmd_deluser,	NULL, LEAF|AUTH},
  {"deop",		"o|o",	 (Function) cmd_deop,		NULL, LEAF|AUTH},
  {"devoice",		"o|o",	 (Function) cmd_devoice,	NULL, LEAF|AUTH},
  {"getkey",            "o|o",   (Function) cmd_getkey,         NULL, LEAF|AUTH},
  {"find",		"",	 (Function) cmd_find,		NULL, LEAF|AUTH},
  {"invite",		"o|o",	 (Function) cmd_invite,		NULL, LEAF|AUTH},
#ifdef CACHE
  {"iop",		"o|o",	 (Function) cmd_iop,		NULL, LEAF|AUTH},
#endif /* CACHE */
  {"kick",		"o|o",	 (Function) cmd_kick,		NULL, LEAF|AUTH},
  {"kickban",		"o|o",	 (Function) cmd_kickban,	NULL, LEAF|AUTH},
  {"mmode",             "n|n",	 (Function) cmd_mmode,		NULL, LEAF|AUTH},
  {"mop",		"n|m",	 (Function) cmd_mop,		NULL, LEAF|AUTH},
  {"msg",		"o",	 (Function) cmd_msg,		NULL, LEAF|AUTH},
  {"nick",		"m",	 (Function) cmd_nick,		NULL, LEAF},
  {"op",		"o|o",	 (Function) cmd_op,		NULL, LEAF|AUTH},
  {"play",		"m|m",	 (Function) cmd_play,		NULL, LEAF|AUTH},
  {"release",		"m",	 (Function) cmd_release,	NULL, LEAF|AUTH},
  {"reset",		"m|m",	 (Function) cmd_reset,		NULL, LEAF|AUTH},
  {"resetbans",		"o|o",	 (Function) cmd_resetbans,	NULL, LEAF|AUTH},
  {"resetexempts",	"o|o",	 (Function) cmd_resetexempts,	NULL, LEAF|AUTH},
  {"resetinvites",	"o|o",	 (Function) cmd_resetinvites,	NULL, LEAF|AUTH},
  {"roles",		"o|o",	 (Function) cmd_roles,		NULL, LEAF},
  {"say",		"o|o",	 (Function) cmd_say,		NULL, LEAF},
  {"swhois",		"",	 (Function) cmd_swhois,		NULL, LEAF|AUTH},
  {"topic",		"o|o",	 (Function) cmd_topic,		NULL, LEAF|AUTH},
  {"voice",		"o|o",	 (Function) cmd_voice,		NULL, LEAF|AUTH},
  {NULL,		NULL,	 NULL,				NULL, 0}
};

/* vim: set sts=2 sw=2 ts=8 et: */
