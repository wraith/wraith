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
 * botmsg.c -- handles:
 *   formatting of messages to be sent on the botnet
 *   sending differnet messages to different versioned bots
 *
 * by Darrin Smith (beldin@light.iinet.net.au)
 *
 */


#include "common.h"
#include "misc.h"
#include "dcc.h"
#include "userrec.h"
#include "main.h"
#include "set.h"
#include "net.h"
#include "users.h"
#include "botmsg.h"
#include "dccutil.h"
#include "cmds.h"
#include "chanprog.h"
#include "botnet.h"
#include "tandem.h"
#include "core_binds.h"
#include <stdarg.h>

static char	OBUF[SGRAB - 110] = "";

void send_uplink(const char *msg, size_t len)
{
  if (uplink_idx == -1 || !valid_idx(uplink_idx) || dcc[uplink_idx].sock == -1 || 
     (dcc[uplink_idx].type != &DCC_BOT) || !dcc[uplink_idx].hub)
    return;

  tputs(dcc[uplink_idx].sock, (char *) msg, len);
}

void send_hubs_but(int idx, const char *msg, size_t len)
{
  int i = 0;

  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type == &DCC_BOT) && i != idx && dcc[i].hub) {
      tputs(dcc[i].sock, (char *) msg, len);
    }
  }
}

/* Ditto for tandem bots
 */
static void send_tand_but(int x, char *buf, size_t len)
{
  int i = 0;

  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type == &DCC_BOT) && i != x) {
      tputs(dcc[i].sock, buf, len);
    }
  }
}

void botnet_send_cmdpass(int idx, char *cmd, char *pass)
{
  if (tands > 0) {
    char *buf = NULL;
    size_t siz = strlen(cmd) + strlen(pass) + 5 + 1;

    buf = (char *) calloc(1, siz);

    size_t len = simple_snprintf(buf, siz, "cp %s %s\n", cmd, pass);
    send_tand_but(idx, buf, len);
    free(buf);
  }
}

int botnet_send_cmd(char * fbot, char * bot, char *fhnd, int fromidx, char * cmd) {
  int i = nextbot(bot);

  if (i >= 0) {
    const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "rc %s %s %s %i %s\n", bot, fbot, fhnd, fromidx, cmd);
    tputs(dcc[i].sock, OBUF, len);
    return 1;
  } else if (!strcasecmp(bot, conf.bot->nick)) {
    char tmp[24] = "";

    simple_snprintf(tmp, sizeof(tmp), "%i", fromidx);
    gotremotecmd(conf.bot->nick, conf.bot->nick, fhnd, tmp, cmd);
  }
  return 0;
}

void botnet_send_cmd_broad(int idx, char * fbot, char *fhnd, int fromidx, char * cmd) {
  if (tands > 0) {
    size_t len = simple_snprintf(OBUF, sizeof OBUF, "rc * %s %s %i %s\n", fbot, fhnd, fromidx, cmd);
    send_tand_but(idx, OBUF, len);
  }
  if (idx < 0) {
    char tmp[24] = "";

    simple_snprintf(tmp, sizeof(tmp), "%i", fromidx);
    gotremotecmd("*", conf.bot->nick, fhnd, tmp, cmd);
  }
}

void botnet_send_cmdreply(char * fbot, char * bot, char * to, char * toidx, char * ln) {
  int i = nextbot(bot);

  if (i >= 0) {
    const size_t len = simple_snprintf(OBUF, sizeof OBUF, "rr %s %s %s %s %s\n", bot, fbot, to, toidx, ln);
    tputs(dcc[i].sock, OBUF, len);
  } else if (!strcasecmp(bot, conf.bot->nick)) {
    gotremotereply(conf.bot->nick, to, toidx, ln);
  }
}


void botnet_send_bye(const char *reason)
{
  if (tands > 0) {
    const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "bye %s\n", reason ? reason : "No reason");

    send_tand_but(-1, OBUF, len);
  }
}

void botnet_send_chan(int idx, char *botnick, char *user, int chan, char *data)
{
  if ((tands > 0) && (chan < GLOBAL_CHANS)) {
    size_t len;

    if (user) {
      len = simple_snprintf2(OBUF, sizeof(OBUF), "c %s@%s %D %s\n", user, botnick, chan, data);
    } else {
      len = simple_snprintf2(OBUF, sizeof(OBUF), "c %s %D %s\n", botnick, chan, data);
    }
    send_tand_but(idx, OBUF, len);
  }
}

void botnet_send_act(int idx, char *botnick, char *user, int chan, char *data)
{
  if ((tands > 0) && (chan < GLOBAL_CHANS)) {
    size_t len;

    if (user) {
      len = simple_snprintf2(OBUF, sizeof(OBUF), "a %s@%s %D %s\n", user, botnick, chan, data);
    } else {
      len = simple_snprintf2(OBUF, sizeof(OBUF), "a %s %D %s\n", botnick, chan, data);
    }
    send_tand_but(idx, OBUF, len);
  }
}

void botnet_send_chat(int idx, const char *botnick, const char *data)
{
  if (tands > 0) {
    const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "ct %s %s\n", botnick, data);

    send_hubs_but(idx, OBUF, len);
  }
}

void botnet_send_ping(int idx)
{
  tputs(dcc[idx].sock, "pi\n", 3);
  dcc[idx].pingtime = now;
}

void botnet_send_pong(int idx)
{
  tputs(dcc[idx].sock, "po\n", 3);
}

void botnet_send_priv (int idx, char *from, char *to, char *tobot, const char *format, ...)
{
  size_t len;
  char tbuf[1024] = "";
  va_list va;

  va_start(va, format);
  egg_vsnprintf(tbuf, sizeof(tbuf), format, va);
  va_end(va);

  if (tobot) {
    len = simple_snprintf(OBUF, sizeof(OBUF), "p %s %s@%s %s\n", from, to, tobot, tbuf);
  } else {
    len = simple_snprintf(OBUF, sizeof(OBUF), "p %s %s %s\n", from, to, tbuf);
  }
  tputs(dcc[idx].sock, OBUF, len);
}

void botnet_send_who(int idx, char *from, char *to, int chan)
{
  const size_t len = simple_snprintf2(OBUF, sizeof(OBUF), "w %s %s %D\n", from, to, chan);

  tputs(dcc[idx].sock, OBUF, len);
}

void botnet_send_unlink(int idx, char *who, char *via, char *bot, char *reason)
{
  const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "ul %s %s %s %s\n", who, via, bot, reason);

  tputs(dcc[idx].sock, OBUF, len);
}

void botnet_send_link(int idx, char *who, char *via, char *bot)
{
  const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "l %s %s %s\n", who, via, bot);

  tputs(dcc[idx].sock, OBUF, len);
}

void botnet_send_unlinked(int idx, char *bot, char *args)
{
  if (tands > 0) {
    const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "un %s %s\n", bot, args ? args : "");

    send_tand_but(idx, OBUF, len);
  }
}

void botnet_send_nlinked(int idx, char *bot, char *next, char flag, int vlocalhub, time_t vbuildts, char *vcommit, char *vversion, int fflags)
{
  if (tands > 0) {
    const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "n %s %s %cD0gc %d %d %s %s %d\n", bot, next, flag,
                                       vlocalhub, (int) vbuildts, vcommit, vversion ? vversion : "", fflags);
    send_tand_but(idx, OBUF, len);
  }
}

void botnet_send_traced(int idx, char *bot, char *buf)
{
  const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "td %s %s\n", bot, buf);

  tputs(dcc[idx].sock, OBUF, len);
}

void botnet_send_trace(int idx, char *to, char *from, char *buf)
{
  const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "t %s %s %s:%s\n", to, from, buf, conf.bot->nick);

  tputs(dcc[idx].sock, OBUF, len);
}

void botnet_send_update(int idx, tand_t * ptr)
{
  if (tands > 0) {
    /* the D0gc is a lingering hack which probably will never be able to come out. */
    const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "u %s %cD0gc %d %d %s %s %d\n", ptr->bot, ptr->share, ptr->localhub,
                                                          (int) ptr->buildts, ptr->commit, ptr->version,
                                                          ptr->fflags);
    send_tand_but(idx, OBUF, len);
  }
}

void botnet_send_reject(int idx, char *fromp, char *frombot, char *top, char *tobot, char *reason)
{
  size_t len;
  char to[NOTENAMELEN + 1] = "", from[NOTENAMELEN + 1] = "";

  if (tobot) {
    simple_snprintf(to, sizeof(to), "%s@%s", top, tobot);
    top = to;
  }
  if (frombot) {
    simple_snprintf(from, sizeof(from), "%s@%s", fromp, frombot);
    fromp = from;
  }
  if (!reason)
    reason = "";
  len = simple_snprintf(OBUF, sizeof(OBUF), "r %s %s %s\n", fromp, top, reason);
  tputs(dcc[idx].sock, OBUF, len);
}

void putallbots(const char *par)
{ 
  botnet_send_zapf_broad(-1, conf.bot->nick, NULL, par);

  return;
}

void putbot(const char *bot, const char *par)
{
  if (!bot || !par || !bot[0] || !par[0])
    return;

  int i = nextbot(bot);

  if (i < 0)
    return;

  botnet_send_zapf(i, conf.bot->nick, bot, par);
}

/*
	botnet_send_log(...)
	 - sends to uplink if a leaf
	 - sends to all linked hubs if a hub
*/
void botnet_send_log(int idx, const char *from, int type, const char *msg, bool truncate_ts)
{
  size_t len;
  // Cut out timestamp
  if (truncate_ts)
    len = simple_snprintf(OBUF, sizeof(OBUF), "lo %s %d %s\n", from, type, &msg[LOG_TS_LEN + 1]); //+1 due to excess space
  else
    len = simple_snprintf(OBUF, sizeof(OBUF), "lo %s %d %s\n", from, type, msg);

  if (conf.bot->hub) {
    send_hubs_but(idx, OBUF, len);
  } else {
    send_uplink(OBUF, len);
  }
}

void botnet_send_zapf(int idx, const char *a, const char *b, const char *c)
{
  const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "z %s %s %s\n", a, b, c);

  tputs(dcc[idx].sock, OBUF, len);
}

void botnet_send_zapf_broad(int idx, const char *a, const char *b, const char *c)
{
  if (tands > 0) {
    const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "zb %s %s%s%s\n", a, b ? b : "", b ? " " : "", c);
    send_tand_but(idx, OBUF, len);
  }
}

void botnet_send_var(int idx, variable_t *var) {
  const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "va %s %s\n", var->name, var->gdata ? var->gdata : "");

  tputs(dcc[idx].sock, OBUF, len);
}

void botnet_send_var_broad(int idx, variable_t *var) {
  if (tands > 0) {
    const size_t len = simple_snprintf(OBUF, sizeof(OBUF), "vab %s %s\n", var->name, var->gdata ? var->gdata : "");

    send_tand_but(idx, OBUF, len);
  }
}

void botnet_send_idle(int idx, char *bot, int sock, int idle, char *away)
{
  if (tands > 0) {
    const size_t len = simple_snprintf2(OBUF, sizeof(OBUF), "i %s %D %Ds%s\n", bot, sock, idle, away ? away : "");

    send_tand_but(idx, OBUF, len);
  }
}

void botnet_send_away(int idx, char *bot, int sock, char *msg, int linking)
{
  if (tands > 0) {
    const size_t len = simple_snprintf2(OBUF, sizeof(OBUF), "aw %s%s %D %s\n", ((idx >= 0) && linking) ? "!" : "", bot, sock, msg ? msg : "");

    send_tand_but(idx, OBUF, len);
  }
}

void botnet_send_join_idx(int useridx)
{
  if (tands > 0) {
    const size_t len = simple_snprintf2(OBUF, sizeof(OBUF), "j %s %s %D %c%D %s\n",
		       conf.bot->nick, dcc[useridx].nick,
		       dcc[useridx].type && dcc[useridx].type == &DCC_RELAYING ? 
                         dcc[useridx].u.relay->chat->channel : dcc[useridx].u.chat->channel, geticon(useridx),
		         dcc[useridx].sock, dcc[useridx].host);

    send_tand_but(-1, OBUF, len);
  }
}

void botnet_send_join_party(int idx, int linking, int useridx)
{
  if (tands > 0) {
    const size_t len = simple_snprintf2(OBUF, sizeof(OBUF), "j %s%s %s %D %c%D %s\n", linking ? "!" : "",
		       party[useridx].bot, party[useridx].nick,
		       party[useridx].chan, party[useridx].flag, party[useridx].sock,
		       party[useridx].from ? party[useridx].from : "");

    send_tand_but(idx, OBUF, len);
  }
}

void botnet_send_part_idx(int useridx, char *reason)
{
  if (tands > 0) {
    const size_t len = simple_snprintf2(OBUF, sizeof(OBUF), "pt %s %s %D %s\n", conf.bot->nick,
			 dcc[useridx].nick, dcc[useridx].sock, reason ? reason : "");

    send_tand_but(-1, OBUF, len);
  }
}

void botnet_send_part_party(int idx, int partyidx, char *reason, int silent)
{
  if (tands > 0) {
    const size_t len = simple_snprintf2(OBUF, sizeof(OBUF), "pt %s%s %s %D %s\n",
		       silent ? "!" : "", party[partyidx].bot,
		       party[partyidx].nick, party[partyidx].sock, reason ? reason : "");

    send_tand_but(idx, OBUF, len);
  }
}

void botnet_send_nkch(int useridx, char *oldnick)
{
  if (tands > 0) {
    const size_t len = simple_snprintf2(OBUF, sizeof(OBUF), "nc %s %D %s\n", conf.bot->nick, dcc[useridx].sock, dcc[useridx].nick);

    send_tand_but(-1, OBUF, len);
  }
}

void botnet_send_nkch_part(int butidx, int useridx, char *oldnick)
{
  if (tands > 0) {
    const size_t len = simple_snprintf2(OBUF, sizeof(OBUF), "nc %s %D %s\n", party[useridx].bot, party[useridx].sock, party[useridx].nick);

    send_tand_but(butidx, OBUF, len);
  }
}

/* This part of add_note is more relevant to the botnet than
 * to the notes file
 */
int add_note(char *to, char *from, char *msg, int idx, int echo)
{
  int i, sock;
  char *p = NULL, botf[81] = "", ss[81] = "", ssf[81] = "";
  struct userrec *u;

  if (strlen(msg) > 450)
    msg[450] = 0;		/* Notes have a limit */
  /* note length + PRIVMSG header + nickname + date  must be <512  */
  p = strchr(to, '@');
  if (p != NULL) {		/* Cross-bot note */
    char x[21] = "";

    *p = 0;
    strlcpy(x, to, sizeof(x));
    x[20] = 0;
    *p = '@';
    p++;
    if (!strcasecmp(p, conf.bot->nick))	/* To me?? */
      return add_note(x, from, msg, idx, echo); /* Start over, dimwit. */
    if (strcasecmp(from, conf.bot->nick)) {
      if (strlen(from) > 40)
	from[40] = 0;
      if (strchr(from, '@')) {
	strlcpy(botf, from, sizeof(botf));
      } else
	simple_snprintf(botf, sizeof(botf), "%s@%s", from, conf.bot->nick);
    } else
      strlcpy(botf, conf.bot->nick, sizeof(botf));
    i = nextbot(p);
    if (i < 0) {
      if (idx >= 0)
	dprintf(idx, "That bot isn't here.\n");
      return NOTE_ERROR;
    }
    if ((idx >= 0) && (echo))
      dprintf(idx, "-> %s@%s: %s\n", x, p, msg);
    if (idx >= 0) {
      simple_snprintf(ssf, sizeof(ssf), "%d:%s", dcc[idx].sock, botf);
      botnet_send_priv(i, ssf, x, p, "%s", msg);
    } else
      botnet_send_priv(i, botf, x, p, "%s", msg);
    return NOTE_OK;		/* Forwarded to the right bot */
  }

  /* Might be form "sock:nick" */
  splitc(ssf, from, ':');
  rmspace(ssf);
  splitc(ss, to, ':');
  rmspace(ss);
  if (!ss[0])
    sock = (-1);
  else
    sock = atoi(ss);

  if (!(u = get_user_by_handle(userlist, to))) {
    if (idx >= 0)
      dprintf(idx, "I don't know anyone by that name.\n");
    return NOTE_ERROR;
  }

  if (is_bot(u))
    return NOTE_ERROR;

  /* Online right now? */
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type->flags & DCT_CHAT) &&
	((sock == (-1)) || (sock == dcc[i].sock)) &&
	(!strcasecmp(dcc[i].nick, to))) {

	char *p2 = NULL, *fr = from;
	int l = 0;
	char work[1024] = "";

	while ((*msg == '<') || (*msg == '>')) {
	  p2 = newsplit(&msg);
	  if (*p2 == '<')
	    l += simple_snprintf(work + l, sizeof(work) - l, "via %s, ", p2 + 1);
	  else if (*from == '@')
	    fr = p2 + 1;
	}
	if (idx == -2 || (!strcasecmp(from, conf.bot->nick)))
	  dprintf(i, "*** [%s] %s%s\n", fr, l ? work : "", msg);
	else
	  dprintf(i, "%cNote [%s]: %s%s\n", 7, fr, l ? work : "", msg);
	if ((idx >= 0) && (echo))
	  dprintf(idx, "-> %s: %s\n", to, msg);
	return NOTE_OK;
    }
  }
  /* The user is not online.. why are we getting a bot note for them? */

  if (idx == (-2))
    return NOTE_OK;		/* Error msg from a tandembot: don't store */

  return NOTE_ERROR;
}
/* vim: set sts=2 sw=2 ts=8 et: */
