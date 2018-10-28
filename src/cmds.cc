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
 * cmds.c -- handles:
 *   commands from a user via dcc
 *   (split in 3, this portion contains no-irc commands)
 *
 */


#include "common.h"
#include "cmds.h"
#include "conf.h"
#include "binary.h"
#include "color.h"
#include "settings.h"
#include "settings.h"
#include "adns.h"
#include "debug.h"
#include "dcc.h"
#include "shell.h"
#include "misc.h"
#include "net.h"
#include "userrec.h"
#include "users.h"
#include "egg_timer.h"
#include "userent.h"
#include "binds.h"
#include "match.h"
#include "main.h"
#include "dccutil.h"
#include "chanprog.h"
#include "botmsg.h"
#include "botcmd.h"	
#include "botnet.h"
#include "tandem.h"
#include "misc_file.h"
#include "help.h"
#include "socket.h"
#include "traffic.h" /* egg_traffic_t */
#include "core_binds.h"
#include "libtcl.h"
#include "src/mod/console.mod/console.h"
#include "src/mod/server.mod/server.h"
#include "src/mod/irc.mod/irc.h"
#include "src/mod/update.mod/update.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/share.mod/share.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Stream.h>
#include <bdlib/src/Array.h>
#include <bdlib/src/base64.h>

#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <signal.h>

extern egg_traffic_t 	traffic;

mycmds 			 cmdlist[300]; /* the list of dcc cmds for help system */
int    			 cmdi = 0;

static char		 *btos(unsigned long);

char s1_10[3] = "",s1_4[3] = "",s1_12[3] = "";

static void tell_who(int idx, int chan)
{
  int i, k, ok = 0, atr = dcc[idx].user ? dcc[idx].user->flags : 0;
  size_t nicklen;
  char format[81] = "";
  char s[1024] = "";

  if (!chan)
    dprintf(idx, "Party line members:  (^ = admin, * = owner, + = master, @ = op)\n");
  else {
    dprintf(idx, "People on channel %s%d:  (^ = admin, * = owner, + = master, @ = op)\n",
                      (chan < GLOBAL_CHANS) ? "" : "*",
                      chan % GLOBAL_CHANS);
  }

  /* calculate max nicklen */
  nicklen = 0;
  for (i = 0; i < dcc_total; i++) {
      if(dcc[i].type && strlen(dcc[i].nick) > nicklen)
          nicklen = strlen(dcc[i].nick);
  }
  if(nicklen < 9) nicklen = 9;
  
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type && dcc[i].type == &DCC_CHAT)
      if (dcc[i].u.chat->channel == chan) {
	if (atr & USER_OWNER) {
	  simple_snprintf(format, sizeof format, "  [%%.2li]  %%c%%-%zus %%s", nicklen);
	  egg_snprintf(s, sizeof(s), format,
		  dcc[i].sock, (geticon(i) == '-' ? ' ' : geticon(i)),
		  dcc[i].nick, dcc[i].host);
	} else {
	  simple_snprintf(format, sizeof format, "  %%c%%-%zus %%s", nicklen);
	  egg_snprintf(s, sizeof(s), format,
		  (geticon(i) == '-' ? ' ' : geticon(i)),
		  dcc[i].nick, dcc[i].host);
	}
	if (atr & USER_MASTER) {
	  if (dcc[i].u.chat->con_flags)
	    simple_sprintf(&s[strlen(s)], " (con:%s)", masktype(dcc[i].u.chat->con_flags));
	}
	if (now - dcc[i].timeval > 300) {
	  unsigned long mydays, hrs, mins;

	  mydays = (now - dcc[i].timeval) / 86400;
	  hrs = ((now - dcc[i].timeval) - (mydays * 86400)) / 3600;
	  mins = ((now - dcc[i].timeval) - (hrs * 3600)) / 60;
	  if (mydays > 0)
	    simple_sprintf(&s[strlen(s)], " (idle %lud%luh)", mydays, hrs);
	  else if (hrs > 0)
	    simple_sprintf(&s[strlen(s)], " (idle %luh%lum)", hrs, mins);
	  else
	    simple_sprintf(&s[strlen(s)], " (idle %lum)", mins);
	}
	dprintf(idx, "%s\n", s);
	if (dcc[i].u.chat->away != NULL)
	  dprintf(idx, "      AWAY: %s\n", dcc[i].u.chat->away);
      }
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type && dcc[i].type == &DCC_BOT) {
      if (!ok) {
	ok = 1;
	dprintf(idx, "Bots connected:\n");
      }
      strftime(s, 20, "%d %b %H:%M %Z", gmtime(&dcc[i].timeval));
      s[20] = 0;
      if (atr & USER_OWNER) {
        simple_snprintf(format, sizeof format, "  [%%.2lu]  %%s%%c%%-%zus (%%s) %%s\n", nicklen);
	dprintf(idx, format,
		dcc[i].sock, dcc[i].status & STAT_CALLED ? "<-" : "->",
		dcc[i].status & STAT_SHARE ? '+' : ' ',
		dcc[i].nick, s, dcc[i].u.bot->version);
      } else {
        simple_snprintf(format, sizeof format, "  %%s%%c%%-%zus (%%s) %%s\n", nicklen);
	dprintf(idx, format,
		dcc[i].status & STAT_CALLED ? "<-" : "->",
		dcc[i].status & STAT_SHARE ? '+' : ' ',
		dcc[i].nick, s, dcc[i].u.bot->version);
      }
    }
  ok = 0;
  for (i = 0; i < dcc_total; i++) {
   if (dcc[i].type) {
    if ((dcc[i].type == &DCC_CHAT) && (dcc[i].u.chat->channel != chan)) {
      if (!ok) {
	ok = 1;
	dprintf(idx, "Other people on the bot:\n");
      }
      if (atr & USER_OWNER) {
	simple_snprintf(format, sizeof format, "  [%%.2lu]  %%c%%-%zus ", nicklen);
	egg_snprintf(s, sizeof(s), format, dcc[i].sock,
		(geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick);
      } else {
	simple_snprintf(format, sizeof format, "  %%c%%-%zus ", nicklen);
	egg_snprintf(s, sizeof(s), format, (geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick);
      }
      if (atr & USER_MASTER) {
	if (dcc[i].u.chat->channel < 0)
	  strlcat(s, "(-OFF-) ", sizeof(s));
	else if (!dcc[i].u.chat->channel)
	  strlcat(s, "(party) ", sizeof(s));
	else
	  simple_snprintf(&s[strlen(s)], sizeof(s) - strlen(s), "(%5d) ", dcc[i].u.chat->channel);
      }
      strlcat(s, dcc[i].host, sizeof(s));
      if (atr & USER_MASTER) {
	if (dcc[i].u.chat->con_flags)
	  simple_sprintf(&s[strlen(s)], " (con:%s)", masktype(dcc[i].u.chat->con_flags));
      }
      if (now - dcc[i].timeval > 300) {
	k = (now - dcc[i].timeval) / 60;
	if (k < 60)
	  simple_sprintf(&s[strlen(s)], " (idle %dm)", k);
	else
	  simple_sprintf(&s[strlen(s)], " (idle %dh%dm)", k / 60, k % 60);
      }
      dprintf(idx, "%s\n", s);
      if (dcc[i].u.chat->away != NULL)
	dprintf(idx, "      AWAY: %s\n", dcc[i].u.chat->away);
    }
   }
  }
}

static void cmd_whom(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# whom %s", dcc[idx].nick, par);

  if (par[0] == '*' || !par[0]) {
    answer_local_whom(idx, -1);
    return;
  }

  int chan = -1;

  if ((par[0] < '0') || (par[0] > '9')) {
    if (chan <= 0) {
      dprintf(idx, "No such channel exists.\n");
      return;
    }
  } else
    chan = atoi(par);
  if ((chan < 0) || (chan > 99999)) {
    dprintf(idx, "Channel number out of range: must be between 0 and 99999.\n");
    return;
  }
  answer_local_whom(idx, chan);
}

static void cmd_botset(int idx, char *par)
{
  char *botnick = NULL;

  if (!par[0] || !(botnick = newsplit(&par))) {
    dprintf(idx, "Usage: botset <bot> [<+/->list] [<var> [data|-]]\n");
    return;
  }

  struct userrec *u = get_user_by_handle(userlist, botnick);

  if (!u || !u->bot) {
    dprintf(idx, "%s is not a valid bot.\n", botnick);
    return;
  }
  
  if (cmd_set_real(botnick, idx, par))
    write_userfile(idx);
  return;
}

static void cmd_set(int idx, char *par)
{
//  if (!par[0]) {
//    dprintf(idx, "Usage: set [<+/->list] <var> [data]\n");
//    return;
//  }
  if (cmd_set_real(NULL, idx, par))
    write_userfile(idx);
  return;
}

static void cmd_cmdpass(int idx, char *par)
{
  char *cmd = NULL, *pass = NULL;

  /* cmdpass [command [newpass]] */
  cmd = newsplit(&par);
  putlog(LOG_CMDS, "*", "#%s# cmdpass %s ...", dcc[idx].nick, cmd[0] ? cmd : "");

  if (!isowner(dcc[idx].nick)) {
    putlog(LOG_WARN, "*", "%s attempted to modify command password for %s - not perm owner", dcc[idx].nick, cmd);
    dprintf(idx, "Perm owners only.\n");
    return;
  }

  pass = newsplit(&par);
  if (!cmd || !cmd[0] || !pass || !pass[0]) {
    dprintf(idx, "Usage: cmdpass <command> <password> [newpassword]\n");
    dprintf(idx, "  Specifying the password but not a new one will clear it.\n");
    return;
  }

  strtolower(cmd);

  if (!findcmd(cmd, 0)) {
    dprintf(idx, "No such DCC command.\n");
    return;
  }

  int has_pass = has_cmd_pass(cmd);

  if (!has_pass && par[0]) {
    dprintf(idx, "That cmd has no password.\n");
    return;
  }

  if (strlen(pass) > MAXPASSLEN)
    pass[MAXPASSLEN] = 0;

  if (has_pass) {
    if (!check_cmd_pass(cmd, pass)) {
      putlog(LOG_WARN, "*", "%s attempted to modify command password for %s - invalid password specified", dcc[idx].nick, cmd);
      dprintf(idx, "Wrong password.\n");
      return;
    }

    if (!par[0]) {
      set_cmd_pass(cmd, 1);
      dprintf(idx, "Removed command password for %s\n", cmd);
      return;
    }
  }

  char *epass = NULL, tmp[256] = "";

  epass = salted_sha1(par[0] ? par : pass);
  simple_snprintf(tmp, sizeof tmp, "%s %s", cmd, epass);
  free(epass);
  if (has_pass)
    dprintf(idx, "Changed command password for %s\n", cmd);
  else
    dprintf(idx, "Set command password for %s to '%s'\n", cmd, par[0] ? par : pass);
  set_cmd_pass(tmp, 1);
  write_userfile(idx);
}

static void cmd_lagged(int idx, char *par)
{
  /* Lists botnet lag to *directly connected* bots */
  putlog(LOG_CMDS, "*", "#%s# lagged %s", dcc[idx].nick, par);
  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].type == &DCC_BOT) {
      dprintf(idx, "%9s - %d seconds\n", dcc[i].nick, (dcc[i].pingtime > 120) ? (int) (now - dcc[i].pingtime) : (int) dcc[i].pingtime);
    }
  }
}

static void cmd_me(int idx, char *par)
{
  if (dcc[idx].simul >= 0) {
    dprintf(idx, "Sorry, that cmd isn't available over botcmd.\n");
    return;
  }

  if (dcc[idx].u.chat->channel < 0) {
    dprintf(idx, "You have chat turned off.\n");
    return;
  }
  if (!par[0]) {
    dprintf(idx, "Usage: me <action>\n");
    return;
  }
  if (dcc[idx].u.chat->away != NULL)
    not_away(idx);
  for (int i = 0; i < dcc_total; i++)
    if (dcc[i].type && (dcc[i].type->flags & DCT_CHAT) && (dcc[i].u.chat->channel == dcc[idx].u.chat->channel) &&
	((i != idx) || (dcc[i].status & STAT_ECHO)))
      dprintf(i, "* %s %s\n", dcc[idx].nick, par);

  botnet_send_act(idx, conf.bot->nick, dcc[idx].nick, dcc[idx].u.chat->channel, par);
}

static void cmd_motd(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# motd %s", dcc[idx].nick, par);
  if (par[0] && (dcc[idx].user->flags & USER_MASTER)) {
    char *s = NULL;
    size_t size;
  
    size = strlen(par) + 1 + strlen(dcc[idx].nick) + 10 + 1 + 1;
    s = (char *) calloc(1, size); /* +2: ' 'x2 */

    simple_snprintf(s, size, "%s %li %s", dcc[idx].nick, (long)now, par);
    var_set_by_name(NULL, "motd", s);
    free(s);
    dprintf(idx, "Motd set\n");
    if (conf.bot->hub)
      write_userfile(idx);
  } else {
    show_motd(idx);
  }
}

static void cmd_about(int idx, char *par)
{
  char c[80] = "";

  putlog(LOG_CMDS, "*", "#%s# about", dcc[idx].nick);
  dprintf(idx, STR("Wraith botpack by bryan\n"));
  dprintf(idx, STR("http://wraith.botpack.net\n"));
  strftime(c, sizeof c, "%c %Z", gmtime(&buildts));
  dprintf(idx, "Version: %s\n", egg_version);
  dprintf(idx, "Build: %s (%li)\n", c, (long)buildts);
  dprintf(idx, "Commit: %s\n", commit);
  dprintf(idx, STR("(written from a base of Eggdrop 1.6.12)\n"));
  dprintf(idx, "..with credits and thanks to the following:\n");
  dprintf(idx, " \n");
  dprintf(idx, STR(" * Eggdev for eggdrop obviously\n"));
  dprintf(idx, STR(" * $blayzkat$b, my wife, for great love, support, ideas and motivation.\n"));
  dprintf(idx, STR(" * $bryguy$b for beta testing, providing code, finding bugs, and providing input.\n"));
  dprintf(idx, STR(" * $bSFC$b for providing compile shells, continuous input, feature suggestions, and testing.\n"));
  dprintf(idx, STR(" * $bwarchest$b for his dedicated bug finding, testing, input, and original inspiration to code a botpack.\n"));
  dprintf(idx, STR(" * $bcontext$b for finding bugs, code, ideas, git, being involved in development.\n"));
  dprintf(idx, STR(" * $bxmage$b for beta testing.\n"));
  dprintf(idx, STR(" * $bpasswd$b for beta testing, and his dedication to finding bugs.\n"));
  dprintf(idx, STR(" * $bpgpkeys$b for finding bugs, and providing input.\n"));
  dprintf(idx, STR(" * $bExcelsior$b for celdrop which inspired many features.\n"));
  dprintf(idx, STR(" * $bsyt$b for giving me inspiration to code a more secure bot.\n"));
  dprintf(idx, STR(" * $bmulder$b for helping with the cookie op algorithm.\n"));
  dprintf(idx, STR(" * $bBlackjac$b for helping with the bx auth script with his Sentinel script.\n"));
  dprintf(idx, STR(" * $bMystikal$b for various bugs.\n"));
  dprintf(idx, STR(" * $bEstella$b for finding bugs, code, ideas, beta testing.\n"));
  dprintf(idx, STR(" * $bDimmiez$b for great ideas.\n"));
  dprintf(idx, STR(" * $bZero$b for a great stream of ideas, beta testing.\n"));
  dprintf(idx, STR(" * $bMafaioz$b for good ideas, support on wraith.no, beta testing.\n"));
  dprintf(idx, STR(" * $bTical$b for great ideas.\n"));
  dprintf(idx, STR(" * $binsect$b for helping test the RBL code.\n"));
  dprintf(idx, STR(" * $bPhillip$b for ideas / code.\n"));
  dprintf(idx, STR(" * $bducch$b for ideas / findings bugs / code.\n"));
  dprintf(idx, STR(" * $bvap0r$b for providing the best raps and beats (http://wepump.in/music)\n"));
  dprintf(idx, STR(" * $bMany$b others.\n"));
  dprintf(idx, " \n");
  dprintf(idx, STR("For a list of Contributors see: git shortlog -sen master\n"));
  dprintf(idx, STR("To Contribute see: http://wraith.botpack.net/wiki/Contributing\n"));
  dprintf(idx, STR("Credit for suggestions/bugs can be found at: http://wraith.botpack.net/report/9\n"));
  dprintf(idx, STR("Bugs can be reported at: http://wraith.botpack.net/newticket\n"));
  dprintf(idx, STR("Bugs can be looked up by #, ie, '#24' in url: http://wraith.botpack.net/ticket/24\n"));
  dprintf(idx, " \n");
  dprintf(idx, STR("Please support wraith by signing up for a shell at http://www.xzibition.com (coupon 'wraith' for 30%% off)\n"));
  dprintf(idx, " \n");
  dprintf(idx, STR("The botpack ghost inspired the early versions of wraith and a few cmds.\n"));
  dprintf(idx, STR("* $beinride$b\n"));
  dprintf(idx, STR("* $bievil$b\n"));
  dprintf(idx, "\n");
  dprintf(idx, STR("The following botpacks gave inspiration, ideas, and some code:\n"));
  dprintf(idx, STR(" * $uawptic$u by $blordoptic$b\n"));
  dprintf(idx, STR(" * $uoptikz$u by $bryguy$b and $blordoptic$b\n"));
  dprintf(idx, STR(" * $uceldrop$u by $bexcelsior$b\n"));
  dprintf(idx, STR(" * $ugenocide$u by $bCrazi$b, $bDor$b, $bpsychoid$b, and $bAce24$b\n"));
  dprintf(idx, STR(" * $utfbot$u by $bwarknite$b and $bloslinux$b\n"));
}

static void cmd_addline(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: addline <user>\n");
    return;
  }

  struct userrec *u = NULL;

  putlog(LOG_CMDS, "*", "#%s# addline %s", dcc[idx].nick, par);

  u = get_user_by_handle(userlist, par);

  if (!u || (u && !whois_access(dcc[idx].user, u))) {
    dprintf(idx, "No such user.\n");
    return;
  }

  struct list_type *q = (struct list_type *) get_user(&USERENTRY_HOSTS, u);
  char *hostbuf = (char *) calloc(1, 1);
  size_t siz = 1;
  
  for (; q; q = q->next) {
    siz = strlen(hostbuf) + strlen(q->extra) + 2;
    hostbuf = (char *) realloc(hostbuf, siz);
    strlcat(hostbuf, q->extra, siz);
    strlcat(hostbuf, " ", siz);
  }
  siz = strlen(hostbuf) + strlen(u->handle) + 19 + 1;

  char *outbuf = (char *) calloc(1, siz);
  simple_snprintf(outbuf, siz, "Addline: +user %s %s", u->handle, hostbuf);
  dumplots(idx, "", outbuf);
  free(hostbuf);
  free(outbuf);
}

static void cmd_away(int idx, char *par)
{
  if (dcc[idx].simul >= 0) {
    dprintf(idx, "Sorry, that cmd isn't available over botcmd.\n");
    return;
  }
  if (strlen(par) > 60)
    par[60] = 0;
  set_away(idx, par);
}

static void cmd_back(int idx, char *par)
{
  if (dcc[idx].simul >= 0) {
    dprintf(idx, "Sorry, that cmd isn't available over botcmd.\n");
    return;
  }
  not_away(idx);
}

static void cmd_newpass(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: newpass <newpassword|rand>\n");
    return;
  }

  char *newpass = newsplit(&par), *pass = NULL;

  putlog(LOG_CMDS, "*", "#%s# newpass...", dcc[idx].nick);

  if (!strcmp(newpass, "rand")) {
    pass = (char*)calloc(1, MAXPASSLEN + 1);
    make_rand_str(pass, MAXPASSLEN);
  } else {
    if (!goodpass(newpass, idx, NULL)) {
      return;
    }
    pass = strdup(newpass);
  }

  set_user(&USERENTRY_PASS, dcc[idx].user, pass);
  dprintf(idx, "Changed your password to: %s\n", pass);
  if (conf.bot->hub)
    write_userfile(idx);
  free(pass);
}

static void cmd_secpass(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: secpass <newsecpass>\nIf you use \"rand\" as the secpass, a random pass will be chosen.\n");
    return;
  }

  char *newpass = newsplit(&par), pass[MAXPASSLEN + 1] = "";

  putlog(LOG_CMDS, "*", "#%s# secpass...", dcc[idx].nick);

  if (!strcmp(newpass, "rand")) {
    make_rand_str(pass, MAXPASSLEN);
  } else {
    if (strlen(newpass) < 6) {
      dprintf(idx, "Please use at least 6 characters.\n");
      return;
    } else
      strlcpy(pass, newpass, sizeof(pass));
  }
  if (strlen(pass) > MAXPASSLEN)
    pass[MAXPASSLEN] = 0;
  set_user(&USERENTRY_SECPASS, dcc[idx].user, pass);
  dprintf(idx, "Changed your secpass to: %s\n", pass);
  if (conf.bot->hub)
    write_userfile(idx);
}

static void cmd_bots(int idx, char *par)
{
  char *node = NULL;
   
  if (par[0])
    node = newsplit(&par);
  putlog(LOG_CMDS, "*", "#%s# bots %s", dcc[idx].nick, node ? node : "");
  tell_bots(idx, 1, node);
}

static void cmd_downbots(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# downbots", dcc[idx].nick);
  tell_bots(idx, 0, NULL);
}


static void cmd_bottree(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# bottree", dcc[idx].nick);
  tell_bottree(idx);
}

static int my_cmp(const mycmds *c1, const mycmds *c2)
{
  return strcmp(c1->name, c2->name);
}

static void cmd_nohelp(int idx, char *par)
{
  char *buf = (char *) calloc(1, 1);
  size_t siz = 1;
  bind_entry_t *entry = NULL;
  bind_table_t *table = bind_table_lookup("dcc");

  buf[0] = 0;

  for (entry = table->entries; entry; entry = entry->next) {
    if (findhelp(entry->mask) == NULL) {
      siz = strlen(buf) + 2 + strlen(entry->mask) + 1;
      buf = (char *) realloc(buf, siz);
      strlcat(buf, entry->mask, siz);
      strlcat(buf, ", ", siz);
    }
  }

  dumplots(idx, "", buf);
}

static int comp_help_t(const void *m1, const void *m2) {
  const help_t *mi1 = (const help_t *) m1;
  const help_t *mi2 = (const help_t *) m2;
  return strcasecmp(mi1->cmd, mi2->cmd);
}

const help_t *
findcmd(const char *lookup, bool care_about_type)
{
  help_t key;
  key.cmd = (char*)lookup;
  help_t *h_entry = (help_t *) bsearch(&key, &help, lengthof(help) - 1, sizeof(help_t), comp_help_t);
  if (h_entry && ((care_about_type && have_cmd(h_entry->cmd, h_entry->type)) || (!care_about_type)))
      return h_entry;

  return NULL;
}

static void cmd_help(int idx, char *par)
{
  char flg[100] = "", *fcats = NULL, temp[100] = "", buf[2046] = "", match[20] = "";
  int fnd = 0, done = 0, nowild = 0;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN | FR_ANYCH, 0, 0, 0 };

  simple_snprintf(temp, sizeof temp, "a|- a|a n|- n|n m|- m|m m|o i|- o|o o|- p|- -|-");
  fcats = temp;

  putlog(LOG_CMDS, "*", "#%s# help %s", dcc[idx].nick, par);
  get_user_flagrec(dcc[idx].user, &fr, NULL);

  build_flags(flg, &fr, NULL);
  if (!par[0]) {
    simple_snprintf(match, sizeof(match), "*");
  } else {
    if (!strchr(par, '*') && !strchr(par, '?'))
      nowild++;
    strlcpy(match, newsplit(&par), sizeof(match));
  }
  if (!nowild)
    dprintf(idx, "Showing help topics matching '%s' for flags: (%s)\n", match, flg);

  qsort(cmdlist, cmdi, sizeof(mycmds), (int (*)(const void *, const void *)) &my_cmp);

  /* even if we have nowild, we loop to conserve code/space */
  while (!done) {
    int i = 0, end = 0, first = 1, n;
    char *flag = NULL;

    flag = newsplit(&fcats);
    if (!flag[0]) 
      done = 1;

    for (n = 0; n < cmdi; n++) { /* loop each command */
      if (!flagrec_ok(&cmdlist[n].flags, &fr) || !wild_match(match, (char *) cmdlist[n].name))
        continue;
      // Auth?
      if (!(!dcc[idx].irc || (dcc[idx].irc && (cmdlist[n].type & AUTH_ALL))))
        continue;
      fnd++;
      if (nowild) {
        flg[0] = 0;
        build_flags(flg, &(cmdlist[n].flags), NULL);
        dprintf(idx, "Showing you help for '%s' (%s):\n", match, flg);
        const help_t *h_entry = NULL;
        if ((h_entry = findhelp(match)) != NULL) {
          if (h_entry->garble_len)
            showhelp(idx, &fr, degarble(h_entry->garble_len, h_entry->desc));
          else
            showhelp(idx, &fr, h_entry->desc);
        }
        done = 1;
        break;
      } else {
        flg[0] = 0;
        build_flags(flg, &(cmdlist[n].flags), NULL);
        if (!strcmp(flg, flag)) {
          if (first) {
            dprintf(idx, "%s\n", buf[0] ? buf : "");
            if (dcc[idx].irc)
              dprintf(idx, "# IRC (%s)\n", flag);
            else
              dprintf(idx, "# DCC (%s)\n", flag);
            simple_snprintf(buf, sizeof(buf), "  ");
          }
          if (end && !first) {
            dprintf(idx, "%s\n", buf[0] ? buf : "");
            /* we dumped the buf to dprintf, now start a new one... */
            simple_snprintf(buf, sizeof(buf), "  ");
          }
          size_t blen = strlen(buf);
          egg_snprintf(&buf[blen], sizeof(buf) - blen, "%-14.14s", cmdlist[n].name);
          first = 0;
          end = 0;
          i++;
        }
        if (i >= 5) {
          end = 1;
          i = 0;
        } 
      }
    }
  }
  if (buf[0])
    dprintf(idx, "%s\n", buf);
  if (fnd) 
    dprintf(idx, "--End help listing\n");
  if (!strcmp(match, "*")) {
    dprintf(idx, "For individual command help, type: %shelp <command>\n", (dcc[idx].u.chat->channel >= 0) ? settings.dcc_prefix : "");
  } else if (!fnd) {
    dprintf(idx, "No match for '%s'.\n", match);
  }
  dprintf(idx, "FAQ: http://wraith.botpack.net/wiki/FrequentlyAskedQuestions\n");
  dprintf(idx, "Documentation: http://wraith.botpack.net/wiki/Documentation\n");
}

static void cmd_addlog(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# addlog %s", dcc[idx].nick, par);
  putlog(LOG_MISC, "*", "%s: %s", dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, "Usage: addlog <message>\n");
    return;
  }
  dprintf(idx, "Placed entry in the log file.\n");
}

static void cmd_who(int idx, char *par)
{
  if (par[0]) {
    if (dcc[idx].u.chat->channel < 0) {
      dprintf(idx, "You have chat turned off.\n");
      return;
    }
    putlog(LOG_CMDS, "*", "#%s# who %s", dcc[idx].nick, par);
    if (!strcasecmp(par, conf.bot->nick))
      tell_who(idx, dcc[idx].u.chat->channel);
    else {
      int i = nextbot(par);

      if (i < 0) {
	dprintf(idx, "That bot isn't connected.\n");
      } else if (dcc[idx].u.chat->channel > 99999)
	dprintf(idx, "You are on a local channel.\n");
      else {
	char s[40] = "";

	simple_snprintf(s, sizeof(s), "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, conf.bot->nick);
	botnet_send_who(i, s, par, dcc[idx].u.chat->channel);
      }
    }
  } else {
    putlog(LOG_CMDS, "*", "#%s# who", dcc[idx].nick);
    if (dcc[idx].u.chat->channel < 0)
      tell_who(idx, 0);
    else
      tell_who(idx, dcc[idx].u.chat->channel);
  }
}

static void cmd_whois(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# whois %s", dcc[idx].nick, par);

  tell_user_ident(idx, par[0] ? par : dcc[idx].nick);
}

static void match(int idx, char *par, int isbot)
{
  putlog(LOG_CMDS, "*", "#%s# match%s %s", dcc[idx].nick, isbot ? "bot" : "", par);

  if (!par[0]) {
    dprintf(idx, "Usage: match%s <%snick/host> [[skip] count]\n", isbot ? "bot" : "", isbot ? "bot" : "");
    return;
  }

  int start = 1, limit = 20;
  char *s = newsplit(&par), *chname = NULL;

  if (strchr(CHANMETA, par[0]) != NULL)
    chname = newsplit(&par);
  else
    chname = "";
  if (atoi(par) > 0) {
    char *s1 = newsplit(&par);

    if (atoi(par) > 0) {
      start = atoi(s1);
      limit = atoi(par);
    } else
      limit = atoi(s1);
  }
  tell_users_match(idx, s, start, limit, chname, isbot);
}

static void cmd_matchbot(int idx, char *par)
{
  match(idx, par, 1);
}

static void cmd_match(int idx, char *par)
{
  match(idx, par, 0);
  dprintf(idx, "- If trying to match a bot, please use '%smatchbot %s'\n", (dcc[idx].u.chat->channel >= 0) ? settings.dcc_prefix : "", par);
}

static void cmd_update(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# update %s", dcc[idx].nick, par);
  if (!conf.bot->hub && !conf.bot->localhub) {
    dprintf(idx, "Please use '%s%s%s' for this login/shell.\n", RED(idx), conf.localhub, COLOR_END(idx));
    return;
  }
  if (!par[0])
    dprintf(idx, "Usage: update <binname>\n");
  updatebin(idx, par, 1);
}

static void cmd_uptime(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# uptime", dcc[idx].nick);
  tell_verbose_uptime(idx);
}

static void print_users(char *work, int idx, int *cnt, int *tt, int bot, int flags, int notflags, const char *str, size_t worksiz)
{
  struct userrec *u = NULL;

  for (u = userlist; u; u = u->next) {
    if (whois_access(dcc[idx].user, u) && 
        ((bot && u->bot) || (!bot && !u->bot)) && 
         ((!flags) || (u->flags & flags)) &&
         ((!notflags) || !(u->flags & notflags)) &&
          (!bot || (bot == 2 && bot_hublevel(u) < 999) || (bot == 1 && bot_hublevel(u) == 999))) {
      if (!*cnt)
        simple_snprintf(work, worksiz, "%-11s: ", str);
      else 
        strlcat(work, ", ", worksiz);

      strlcat(work, u->handle, worksiz);
      (*cnt)++;
      (*tt)++;
      if (*cnt == 11) {
        dprintf(idx, "%s\n", work);
        work[0] = 0;
        *cnt = 0;
      }
    }
  }
  if (work[0])
    dprintf(idx, "%s\n", work);

  work[0] = 0;
  *cnt = 0;
}

#define PRINT_USERS(bot, flags, notflags, str)	print_users(work, idx, &cnt, &tt, bot, flags, notflags, str, sizeof(work))

static void cmd_userlist(int idx, char *par)
{
  int cnt = 0, tt = 0, bt = 0;
  char work[200] = "";

  putlog(LOG_CMDS, "*", "#%s# userlist", dcc[idx].nick);
  
  PRINT_USERS(2, 0, 0, "Hubs");
  PRINT_USERS(1, 0, BOT_BACKUP, "Main Bots");
  PRINT_USERS(1, BOT_BACKUP, 0, "Backup Bots");
  bt = tt;			/* we don't want to add these duplicates into the total */
  PRINT_USERS(1, BOT_CHANHUB, 0, "Chatbots");
  PRINT_USERS(1, BOT_DOVOICE, 0, "Voicebots");
  PRINT_USERS(1, BOT_DOLIMIT, 0, "Limitbots");
  PRINT_USERS(1, BOT_DORESOLV, 0, "Resolvbots");
  PRINT_USERS(1, BOT_FLOODBOT, 0, "Floodbots");
  tt = 0;
  PRINT_USERS(0, USER_ADMIN, 0, "Admins");
  PRINT_USERS(0, USER_OWNER, USER_ADMIN, "Owners");
  PRINT_USERS(0, USER_MASTER, USER_OWNER, "Masters");
  PRINT_USERS(0, USER_OP, USER_MASTER, "Ops");
  PRINT_USERS(0, 0, USER_OP, "Users");

  dprintf(idx, "Total bots : %d\n", bt);
  dprintf(idx, "Total users: %d\n", tt);

  return;
}

static void cmd_groups(int idx, char *par)
{
  struct userrec *u = NULL;

  putlog(LOG_CMDS, "*", "#%s# groups %s", dcc[idx].nick, par);
  bd::String botnick(newsplit(&par));

  if (botnick.length() && !(u = get_user_by_handle(userlist, botnick.c_str()))) {
    dprintf(idx, "No such bot.\n");
    return;
  }

  if (u && !u->bot) {
    dprintf(idx, "%s is not a bot.\n", botnick.c_str());
    return;
  }

  bd::Array<bd::String> globalgroups = bd::String(var_get_gdata("groups")).split(",");
  bd::Array<bd::String> allgroups;
  bd::HashTable<bd::String, bd::Array<bd::String> > groupBots;
  bd::HashTable<bd::String, bd::Array<bd::String> > botGroups;
  size_t maxGroupLen = 0;

  // Need to loop over every bot and make a list of all groups and bots which are in those groups
  for (u = userlist; u; u = u->next) {
    if (u->bot && bot_hublevel(u) == 999) {
      // Gather all the groups for this bot
      botGroups[u->handle] = bd::String(var_get_bot_data(u, "groups", true)).split(",");
      for (size_t i = 0; i < botGroups[u->handle].length(); ++i) {
        const bd::String group(botGroups[u->handle][i]);
        if (group.length() > maxGroupLen) {
          maxGroupLen = group.length();
        }
        // Add their groups into the master list
        if (allgroups.find(group) == allgroups.npos) {
          allgroups << group;
        }
        // Add them to the list for this group
        groupBots[group] << u->handle;

      }
    }
  }

  if (botnick.length()) {
    dprintf(idx, "%s is in groups: %s\n", botnick.c_str(), static_cast<bd::String>(botGroups[botnick].join(" ")).c_str());
    dprintf(idx, "Total groups: %zu/%zu\n", botGroups[botnick].length(), allgroups.length());
  } else {
    // Display all groups and which bots are in them
    for (size_t i = 0; i < allgroups.length(); ++i) {
      const bd::String group(allgroups[i]);
      const bd::Array<bd::String> bots(groupBots[group]);
      dumplots(idx, bd::String::printf("%-*s: ", int(maxGroupLen), group.c_str()).c_str(), static_cast<bd::String>(bots.join(" ")).c_str());
    }
    dprintf(idx, "Total groups: %zu\n", allgroups.length());
  }

  return;
}

static void cmd_channels(int idx, char *par) {
  putlog(LOG_CMDS, "*", "#%s# channels %s", dcc[idx].nick, par);
  if (par[0] && (dcc[idx].user->flags & USER_MASTER)) {
    if (par[0] == '%') {
      show_channels(idx, par);
    } else {
      struct userrec *user = get_user_by_handle(userlist, par);
      if (user && whois_access(dcc[idx].user, user)) {
        show_channels(idx, par);
      } else  {
        dprintf(idx, "No such user.\n");
      }
    }
  } else {
      show_channels(idx, NULL);
  }

  if ((dcc[idx].user->flags & USER_MASTER) && !par[0])
    dprintf(idx, "You can also %schannels <user|%%group>\n", (dcc[idx].u.chat->channel >= 0) ? settings.dcc_prefix : "");
}


void channels_report(int, int);
void transfer_report(int, int);
void share_report(int, int);
void update_report(int, int);

static void cmd_status(int idx, char *par)
{
  int atr = dcc[idx].user ? dcc[idx].user->flags : 0, all = 0;

  if (!strcasecmp(par, "all")) {
    if (!(atr & USER_MASTER)) {
      dprintf(idx, "You do not have Bot Master privileges.\n");
      return;
    }
    putlog(LOG_CMDS, "*", "#%s# status all", dcc[idx].nick);
    tell_verbose_status(idx);
    dprintf(idx, "\n");
    tell_settings(idx);
    all++;
  } else {
    putlog(LOG_CMDS, "*", "#%s# status", dcc[idx].nick);
    tell_verbose_status(idx);
  }
  if (!conf.bot->hub) {
    server_report(idx, all);
    irc_report(idx, all);
  }
  channels_report(idx, all);
  transfer_report(idx, all);
  share_report(idx, all);
  update_report(idx, all);
}

static void cmd_dccstat(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# dccstat", dcc[idx].nick);
  tell_dcc(idx);
}

static void cmd_boot(int idx, char *par)
{
  char *who = NULL;
  int i, ok = 0;

  if (!par[0]) {
    dprintf(idx, "Usage: boot nick[@bot]\n");
    return;
  }
  who = newsplit(&par);
  if (strchr(who, '@') != NULL) {
    char whonick[HANDLEN + 1];

    splitcn(whonick, who, '@', sizeof(whonick));
    if (!strcasecmp(who, conf.bot->nick)) {
      cmd_boot(idx, whonick);
      return;
    }
      i = nextbot(who);
      if (i < 0) {
        dprintf(idx, "No such bot connected.\n");
        return;
      }
      botnet_send_reject(i, dcc[idx].nick, conf.bot->nick, whonick, who, par[0] ? par : dcc[idx].nick);
      putlog(LOG_BOTS, "*", "#%s# boot %s@%s (%s)", dcc[idx].nick, whonick, who, par[0] ? par : dcc[idx].nick);
    return;
  }
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].simul == -1 && (dcc[i].type->flags & DCT_CANBOOT) && !strcasecmp(dcc[i].nick, who)) {
      if (!whois_access(dcc[idx].user, dcc[i].user)) {
        dprintf(idx, "Sorry, you cannot boot %s.\n", dcc[i].nick);
        return;
      }
      dprintf(idx, "Booting %s from the party line.\n", dcc[i].nick);
      putlog(LOG_CMDS, "*", "#%s# boot %s %s", dcc[idx].nick, who, par);
      do_boot(i, dcc[idx].nick, par);
      ok = 1;
    }
  }
  if (!ok)
    dprintf(idx, "Who?  No such person on the party line.\n");
}

static void cmd_console(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Your console is %s: %s (%s).\n",
	    dcc[idx].u.chat->con_chan,
	    masktype(dcc[idx].u.chat->con_flags),
	    maskname(dcc[idx].u.chat->con_flags));
    return;
  }

  char *nick = NULL, s[2] = "", s1[512] = "";
  int dest = 0, i, ok = 0, pls, md;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  struct chanset_t *chan = NULL;

  get_user_flagrec(dcc[idx].user, &fr, dcc[idx].u.chat->con_chan);
  strlcpy(s1, par, sizeof(s1));
  nick = newsplit(&par);
  /* Don't remove '+' as someone couldn't have '+' in CHANMETA cause
   * he doesn't use IRCnet ++rtc.
   */
  if (nick[0] && !strchr(CHANMETA "+-*", nick[0]) && glob_master(fr)) {
    for (i = 0; i < dcc_total; i++) {
      if (dcc[i].type && !strcasecmp(nick, dcc[i].nick) && (dcc[i].type == &DCC_CHAT) && (!ok)) {
	ok = 1;
	dest = i;
      }
    }
    if (!ok) {
      dprintf(idx, "No such user on the party line!\n");
      return;
    }
    nick[0] = 0;
  } else
    dest = idx;
  if (!nick[0])
    nick = newsplit(&par);
  /* Consider modeless channels, starting with '+' */
  if ((nick [0] == '+' && findchan_by_dname(nick)) ||
      (nick [0] != '+' && strchr(CHANMETA "*", nick[0]))) {
    chan = findchan_by_dname(nick);

    get_user_flagrec(dcc[idx].user, &fr, nick);

    if (strcmp(nick, "*") && (!chan || privchan(fr, chan, PRIV_OP))) {
      dprintf(idx, "Invalid console channel: %s.\n", nick);
      return;
    }

    if (!chk_op(fr, chan)) {
      dprintf(idx, "You don't have op or master access to channel %s.\n", nick);
      return;
    }

    strlcpy(dcc[dest].u.chat->con_chan, nick, sizeof(dcc[dest].u.chat->con_chan));
    nick[0] = 0;
    if ((dest == idx) && !glob_master(fr) && !chan_master(fr))
      /* Consoling to another channel for self */
      dcc[dest].u.chat->con_flags &= ~(LOG_MISC | LOG_CMDS | LOG_WALL);
  }
  if (!nick[0])
    nick = newsplit(&par);
  pls = 1;
  if (nick[0]) {
    if ((nick[0] != '+') && (nick[0] != '-'))
      dcc[dest].u.chat->con_flags = 0;
    for (; *nick; nick++) {
      if (*nick == '+')
	pls = 1;
      else if (*nick == '-')
	pls = 0;
      else {
	s[0] = *nick;
	s[1] = 0;
	md = logmodes(s);
	if ((dest == idx) && !glob_master(fr) && pls) {
	  if (chan_master(fr))
	    md &= ~(LOG_FILES | LOG_DEBUG);
	  else
	    md &= ~(LOG_MISC | LOG_CMDS | LOG_FILES | LOG_WALL | LOG_DEBUG);
	}
        if (!isowner(dcc[dest].nick) && pls)
          md &= ~(LOG_RAW | LOG_BOTNET | LOG_BOTSHARE);
        if (!glob_admin(fr) && pls)
          md &= ~(LOG_SRVOUT | LOG_DEBUG);
	if (!glob_owner(fr) && pls)
	  md &= ~(LOG_ERRORS | LOG_GETIN | LOG_WARN);
	if (!glob_master(fr) && pls)
	  md &= ~LOG_BOTS;
	if (pls)
	  dcc[dest].u.chat->con_flags |= md;
	else
	  dcc[dest].u.chat->con_flags &= ~md;
      }
    }
  }
  putlog(LOG_CMDS, "*", "#%s# console %s", dcc[idx].nick, s1);
  if (dest == idx) {
    dprintf(idx, "Set your console to %s: %s (%s).\n",
	    dcc[idx].u.chat->con_chan,
	    masktype(dcc[idx].u.chat->con_flags),
	    maskname(dcc[idx].u.chat->con_flags));
  } else {
    dprintf(idx, "Set console of %s to %s: %s (%s).\n", dcc[dest].nick,
	    dcc[dest].u.chat->con_chan,
	    masktype(dcc[dest].u.chat->con_flags),
	    maskname(dcc[dest].u.chat->con_flags));
    dprintf(dest, "%s set your console to %s: %s (%s).\n", dcc[idx].nick,
	    dcc[dest].u.chat->con_chan,
	    masktype(dcc[dest].u.chat->con_flags),
	    maskname(dcc[dest].u.chat->con_flags));
  }
  console_dostore(dest);
}

static void cmd_date(int idx, char *par)
{
  char date[50] = "";
  time_t hub;

  putlog(LOG_CMDS, "*", "#%s# date", dcc[idx].nick);

  strftime(date, sizeof date, "%c %Z", localtime(&now));
  dprintf(idx, "%s (local shell time)\n", date);
  strftime(date, sizeof date, "%c %Z", gmtime(&now));
  dprintf(idx, "%s <-- This time is used on the bot.\n", date);
  
  hub = now + timesync;
  strftime(date, sizeof date, "%c %Z", gmtime(&hub));
  dprintf(idx, "%s <-- Botnet uses this\n", date);
}

static void cmd_chhandle(int idx, char *par)
{
  char hand[HANDLEN + 1] = "", newhand[HANDLEN + 1] = "";

  strlcpy(hand, newsplit(&par), sizeof hand);
  strlcpy(newhand, newsplit(&par), sizeof newhand);

  if (!hand[0] || !newhand[0]) {
    dprintf(idx, "Usage: chhandle <oldhandle> <newhandle>\n");
    return;
  }

  struct userrec *u2 = get_user_by_handle(userlist, hand);

  if (!u2 || (u2 && !whois_access(dcc[idx].user, u2))) {
    dprintf(idx, "No such user!\n");
    return;
  }

  if (u2->bot) {
    dprintf(idx, "Sorry, bot handles cannot be changed from partyline.\n");
    return;
  }


  for (size_t i = 0; i < strlen(newhand); i++)
    if ((newhand[i] <= 32) || (newhand[i] >= 127) || (newhand[i] == '@'))
      newhand[i] = '?';
  if (isowner(hand)) {
    dprintf(idx, "Sorry, perm owners may not change their handle without recompiling binaries.\n");
    return;
  } else if (strchr(BADHANDCHARS, newhand[0]) != NULL)
    dprintf(idx, "Bizarre quantum forces prevent nicknames from starting with '%c'.\n", newhand[0]);
  else if (get_user_by_handle(userlist, newhand) && strcasecmp(hand, newhand))
    dprintf(idx, "Somebody is already using %s.\n", newhand);
  else {
    int atr = dcc[idx].user ? dcc[idx].user->flags : 0, atr2 = u2 ? u2->flags : 0;

    if (!(atr & USER_MASTER) && !u2->bot)
      dprintf(idx, "You can't change handles for non-bots.\n");
    else if (u2->bot && !(atr & USER_OWNER))
      dprintf(idx, "You can't change a bot's nick.\n");
    else if ((atr2 & USER_OWNER) && !(atr & USER_OWNER) &&
            strcasecmp(dcc[idx].nick, hand))
      dprintf(idx, "You can't change a bot owner's handle.\n");
    else if (isowner(hand) && strcasecmp(dcc[idx].nick, hand))
      dprintf(idx, "You can't change a permanent bot owner's handle.\n");
    else if (!strcasecmp(newhand, conf.bot->nick) && !(u2->bot || nextbot(hand) != -1))
      dprintf(idx, "Hey! That's MY name!\n");
    else if (change_handle(u2, newhand)) {
      putlog(LOG_CMDS, "*", "#%s# chhandle %s %s", dcc[idx].nick,
            hand, newhand);
      dprintf(idx, "Changed.\n");
      write_userfile(idx);
    } else
      dprintf(idx, "Failed.\n");
  }
}

static void cmd_handle(int idx, char *par)
{
  char newhandle[HANDLEN + 1] = "";

  strlcpy(newhandle, newsplit(&par), sizeof newhandle);

  if (!newhandle[0]) {
    dprintf(idx, "Usage: handle <new-handle>\n");
    return;
  }

  for (size_t i = 0; i < strlen(newhandle); i++)
    if ((newhandle[i] <= 32) || (newhandle[i] >= 127) || (newhandle[i] == '@'))
      newhandle[i] = '?';
  if (isowner(dcc[idx].nick)) {
    dprintf(idx, "Sorry, perm owners may not change their handle without recompiling binaries.\n");
    return;
  } else if (strchr(BADHANDCHARS, newhandle[0]) != NULL) {
    dprintf(idx, "Bizarre quantum forces prevent handle from starting with '%c'.\n", newhandle[0]);
  } else if (get_user_by_handle(userlist, newhandle) && strcasecmp(dcc[idx].nick, newhandle)) {
    dprintf(idx, "Somebody is already using %s.\n", newhandle);
  } else if (!strcasecmp(newhandle, conf.bot->nick)) {
    dprintf(idx, "Hey!  That's MY name!\n");
  } else {
    char oldhandle[HANDLEN + 1] = "";

    strlcpy(oldhandle, dcc[idx].nick, sizeof oldhandle);
    if (change_handle(dcc[idx].user, newhandle)) {
      putlog(LOG_CMDS, "*", "#%s# handle %s", oldhandle, newhandle);
      dprintf(idx, "Okay, changed.\n");
      if (conf.bot->hub)
        write_userfile(idx);
    } else
      dprintf(idx, "Failed.\n");
  }
}

static void cmd_chpass(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: chpass <handle> [password|rand]\n");
    return;
  }
  char *handle = newsplit(&par);
  struct userrec *u = get_user_by_handle(userlist, handle);

  if (!u || (u && !whois_access(dcc[idx].user, u)))
    dprintf(idx, "No such user.\n");
  else if (u->bot)
    dprintf(idx, "Bots do not have passwords.\n");
  else if (!par[0]) {
    putlog(LOG_CMDS, "*", "#%s# chpass %s [nothing]", dcc[idx].nick, handle);
    set_user(&USERENTRY_PASS, u, NULL);
    dprintf(idx, "Removed password.\n");
  } else {
    bool randpass = 0;
    char *newpass = newsplit(&par), *pass = NULL;

    if (!strcmp(newpass, "rand")) {
      pass = (char*)calloc(1, MAXPASSLEN + 1);
      make_rand_str(pass, MAXPASSLEN);
      randpass = 1;
    } else {
      if (!goodpass(newpass, idx, NULL)) {
        return;
      }
      pass = strdup(newpass);
    }

    set_user(&USERENTRY_PASS, u, pass);
    putlog(LOG_CMDS, "*", "#%s# chpass %s [%s]", dcc[idx].nick, handle, randpass ? "random" : "something");
    dprintf(idx, "Password for '%s' changed to: %s\n", handle, pass);
    write_userfile(idx);

    free(pass);
  }
}

static void cmd_chsecpass(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: chsecpass <handle> [secpass/rand]\n");
    return;
  }
  char *handle = newsplit(&par);
  struct userrec *u = get_user_by_handle(userlist, handle);

  if (!u || (u && !whois_access(dcc[idx].user, u)))
    dprintf(idx, "No such user.\n");
  else if (u->bot)
    dprintf(idx, "Bots do not have secpasses.\n");
//  int atr = dcc[idx].user ? dcc[idx].user->flags : 0;
//  else if ((u->flags & USER_OWNER) && !(atr & USER_OWNER) && strcasecmp(handle, dcc[idx].nick))
//    dprintf(idx, "You can't change a bot owner's secpass.\n");
//  else if (isowner(handle) && strcasecmp(dcc[idx].nick, handle))
//    dprintf(idx, "You can't change a permanent bot owner's secpass.\n");
  else if (!par[0]) {
    putlog(LOG_CMDS, "*", "#%s# chsecpass %s [nothing]", dcc[idx].nick, handle);
    set_user(&USERENTRY_SECPASS, u, NULL);
    dprintf(idx, "Removed secpass.\n");
  } else {
    char *newpass = newsplit(&par), pass[MAXPASSLEN + 1] = "";
    size_t l = strlen(newpass);
    bool randpass = 0;

    if (l > MAXPASSLEN)
      newpass[MAXPASSLEN] = 0;
    if (!strcmp(newpass, "rand")) {
      make_rand_str(pass, MAXPASSLEN);
      randpass = 1;
    } else {
      if (strlen(newpass) < 6) {
        dprintf(idx, "Please use at least 6 characters.\n");
        return;
      } else
        strlcpy(pass, newpass, sizeof(pass));
    }
    if (strlen(pass) > MAXPASSLEN)
      pass[MAXPASSLEN] = 0;
    set_user(&USERENTRY_SECPASS, u, pass);
    putlog(LOG_CMDS, "*", "#%s# chsecpass %s [%s]", dcc[idx].nick, handle, randpass ? "random" : "something");
    dprintf(idx, "Secpass for '%s' changed to: %s\n", handle, pass);
    write_userfile(idx);
  }
}

static void cmd_botcmd(int idx, char *par)
{
  if (dcc[idx].simul >= 0) {
    dprintf(idx, "Sorry, you can't chain '%sbotcmd'.\n", (dcc[idx].u.chat->channel >= 0) ? settings.dcc_prefix : "");
    putlog(LOG_WARN, "*", "%s attempted to chain 'botcmd' over the botnet.", dcc[idx].nick);
    return;
  }

  char *botm = newsplit(&par), *group = NULL, *cmd = NULL;
  bool rand_leaf = 0, all_localhub = 0;
  
  if (par[0]) {
    // Group specified
    if (par[0] == '%') {
      group = newsplit(&par);
      ++group;
    }
  }
  if (par[0]) {
    cmd = newsplit(&par);
  }

  if (!botm[0] || !cmd || (cmd && !cmd[0])) {
    dprintf(idx, "Usage: botcmd <bot|*|?|&> [%%group] <cmd> [params]\n");
    return;
  }

  int cnt = 0, rleaf = -1, tbots = 0, found = 0;
  bool group_match = false;
  tand_t *tbot = NULL;

  /* the rest of the cmd will be logged remotely */
  if (strcmp(botm, "?"))
    putlog(LOG_CMDS, "*", "#%s# botcmd %s %s ...", dcc[idx].nick, botm, cmd);

  // Restrict dangerous mass commands ('botcmd *' (any *) or 'botcmd &')
  if ((strchr(botm, '*') && !findbot(botm)) || !strcmp(botm, "&") || botm[0] == '%') {
    if (!strncasecmp(cmd, "di", 2) || (!strncasecmp(cmd, "res", 3) && strncasecmp(cmd, "reset", 5)) || !strncasecmp(cmd, "sui", 3) || !strncasecmp(cmd, "pl", 2) || !strncasecmp(cmd, "ac", 2) ||
        !strncasecmp(cmd, "j", 1) || (!strncasecmp(cmd, "dump", 4) && (!strncasecmp(par, "privmsg", 7) || !strncasecmp(par, "notice", 6) || !strncasecmp(par, "quit", 4))) ||
        ((!strncasecmp(cmd, "tcl", 3) || !strncasecmp(cmd, "script", 6)) && strstr(par, "privmsg"))) {
      dprintf(idx, "Not a good idea.\n");
      return;
    } else if (strchr(botm, '*') && !(dcc[idx].user->flags & USER_OWNER)) {
      dprintf(idx, "'botcmd *' is limited to +n only.\n");
      return;
    }
  }

  /* random leaf */
  if (!strcmp(botm, "?")) {
    rand_leaf = 1;
    for (tbot = tandbot; tbot; tbot = tbot->next) {
      if (bot_hublevel(tbot->u) == 999) {
        if (group) {
          group_match = bd::String(var_get_bot_data(tbot->u, "groups", true)).split(',').find(group) != bd::String::npos;
        } else {
          group_match = true;
        }
        if (group_match) {
          tbots++;
        }
      }
    }
    if (tbots)
      rleaf = 1 + randint(tbots);		/* 1 <--> tbots */
  }

  /* localhubs */
  if (!strcmp(botm, "&")) {
    if (!(dcc[idx].user->flags & USER_OWNER)) {
      dprintf(idx, "'botcmd &' is limited to +n only.\n");
      return;
    }
    all_localhub = 1;
    for (tbot = tandbot; tbot; tbot = tbot->next) {
      if (bot_hublevel(tbot->u) == 999 && tbot->localhub) {
        if (group) {
          group_match = bd::String(var_get_bot_data(tbot->u, "groups", true)).split(',').find(group) != bd::String::npos;
        } else {
          group_match = true;
        }
        if (group_match) {
          tbots++;
        }
      }
    }
  }
  
  for (tbot = tandbot; tbot; tbot = tbot->next) {
    if ((rand_leaf && bot_hublevel(tbot->u) != 999) ||
        (all_localhub && (bot_hublevel(tbot->u) != 999 || !tbot->localhub)))
      continue;
    group_match = false;
    if (group && bot_hublevel(tbot->u) == 999) {
      group_match = bd::String(var_get_bot_data(tbot->u, "groups", true)).split(',').find(group) != bd::String::npos;
    } else if (!group) {
      group_match = true;
    }
    if (group_match) {
      cnt++;
      if ((rleaf != -1 && cnt == rleaf) || (rleaf == -1 && (all_localhub || wild_match(botm, tbot->bot)))) {
        if (rleaf != -1)
          putlog(LOG_CMDS, "*", "#%s# botcmd %s %s ...", dcc[idx].nick, tbot->bot, cmd);
        send_remote_simul(idx, tbot->bot, cmd, par ? par : (char *) "");
        found++;
      }
    }
  }

  /* Only wild_match when not using botm=? and strlen(conf.bot->nick) > 1, ie dont match on hubs with 1 letter nicks */
  if ((!(strlen(conf.bot->nick) == 1 && !strcmp(botm, "?")) && wild_match(botm, conf.bot->nick)) || !strcasecmp(botm, conf.bot->nick)) {
    found++;
    check_bind_dcc(cmd, idx, par);
  }

  if (!found) {
    if (group)
      dprintf(idx, "No linked bots found in group '%s'\n", group);
    else
      dprintf(idx, "No bot matching '%s' linked\n", botm);
  }

  return;
}

static void cmd_hublevel(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# hublevel %s", dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, "Usage: hublevel <bot> <level>\n");
    return;
  }

  char *handle = NULL, *level = NULL;
  struct userrec *u1 = NULL;

  handle = newsplit(&par);

  if (!par[0]) {
    dprintf(idx, "Usage: hublevel <bot> <level>\n");
    dprintf(idx, "A hublevel is required.\n");
    return;
  }

  level = newsplit(&par);
  u1 = get_user_by_handle(userlist, handle);
  if (!u1 || !u1->bot) {
    dprintf(idx, "Useful only for bots.\n");
    return;
  }

  struct bot_addr *bi = NULL, *obi = NULL;

  dprintf(idx, "Changed bot's hublevel.\n");
  obi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, u1);
  bi = (struct bot_addr *) calloc(1, sizeof(struct bot_addr));

  bi->uplink = strdup(obi->uplink);
  bi->address = strdup(obi->address);
  bi->telnet_port = obi->telnet_port;
  bi->relay_port = obi->relay_port;
  bi->hublevel = atoi(level);
  set_user(&USERENTRY_BOTADDR, u1, bi);
  write_userfile(idx);
}

static void cmd_uplink(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# uplink %s", dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, "Usage: uplink <bot> [uplink]\n");
    return;
  }

  char *handle = NULL, *uplink = NULL;
  struct userrec *u1 = NULL;

  handle = newsplit(&par);
  uplink = newsplit(&par);
  if (!uplink)
    uplink = "";
  u1 = get_user_by_handle(userlist, handle);
  if (!u1 || !u1->bot) {
    dprintf(idx, "Useful only for bots.\n");
    return;
  }
  if (uplink && uplink[0])
    dprintf(idx, "Changed bot's uplink.\n");
  else
    dprintf(idx, "Cleared bot's uplink.\n");

  struct bot_addr *bi = NULL, *obi = NULL;

  obi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, u1);
  bi = (struct bot_addr *) calloc(1, sizeof(struct bot_addr));

  bi->uplink = strdup(uplink);
  bi->address = strdup(obi->address);
  bi->telnet_port = obi->telnet_port;
  bi->relay_port = obi->relay_port;
  bi->hublevel = obi->hublevel;
  set_user(&USERENTRY_BOTADDR, u1, bi);
  write_userfile(idx);
}

static void cmd_chaddr(int idx, char *par)
{
  char *handle = NULL;

  handle = newsplit(&par);
  if (!par[0]) {
    dprintf(idx, "Usage: chaddr <bot> <address[:telnet-port[/relay-port]]>\n");
    return;
  }

  char *addr = NULL;
  struct userrec *u1 = NULL;

  addr = newsplit(&par);
  if (strlen(addr) > UHOSTMAX)
    addr[UHOSTMAX] = 0;
  u1 = get_user_by_handle(userlist, handle);
  if (!u1 || !u1->bot) {
    dprintf(idx, "This command is only useful for tandem bots.\n");
    return;
  }
  if ((u1 && u1->bot) && (!dcc[idx].user || !(dcc[idx].user->flags & USER_OWNER))) {
    dprintf(idx, "You can't change a bot's address.\n");
    return;
  }
  putlog(LOG_CMDS, "*", "#%s# chaddr %s %s", dcc[idx].nick, handle, addr);
  dprintf(idx, "Changed bot's address.\n");

  in_port_t telnet_port = 3333, relay_port = 3333;
  char *p = NULL, *q = NULL;
#ifdef USE_IPV6
  char *r = NULL;
#endif /* USE_IPV6 */
  struct bot_addr *bi = NULL, *obi = NULL;

  obi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, u1);
  bi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, u1);
  if (bi) {
    telnet_port = bi->telnet_port;
    relay_port = bi->relay_port;
  }

  bi = (struct bot_addr *) calloc(1, sizeof(struct bot_addr));

  bi->uplink = strdup(obi->uplink);
  bi->hublevel = obi->hublevel;

  q = strchr(addr, ':');

  /* no port */
  if (!q) {
    bi->address = strdup(addr);
    bi->telnet_port = telnet_port;
    bi->relay_port = relay_port;

  /* with a port, or ipv6 ip */
  } else {
#ifdef USE_IPV6
    if ((r = strchr(addr, '['))) {		/* ipv6 notation [3ffe:80c0:225::] */
      addr++;					/* lose the '[' */
      r = strchr(addr, ']');			/* pointer to the ending ']' */

      bi->address = (char *) calloc(1, r - addr + 1);	/* alloc and copy the addr */
      strlcpy(bi->address, addr, r - addr + 1);

      q = r + 1;				/* set q to ':' at addr */
    } else {
#endif /* !USE_IPV6 */
      bi->address = (char *) calloc(1, q - addr + 1);
      strlcpy(bi->address, addr, q - addr + 1);
#ifdef USE_IPV6
    }
#endif /* USE_IPV6 */

    /* port */
    p = q + 1;
    bi->telnet_port = atoi(p);
    q = strchr(p, '/');
    if (!q)
      bi->relay_port = bi->telnet_port;
    else
      bi->relay_port = atoi(q + 1);
  }
  set_user(&USERENTRY_BOTADDR, u1, bi);
  write_userfile(idx);
}

static void cmd_comment(int idx, char *par)
{
  char *handle = NULL;

  handle = newsplit(&par);

  if (!par[0]) {
    dprintf(idx, "Usage: comment <handle> <newcomment/none>\n");
    return;
  }

  struct userrec *u1 = get_user_by_handle(userlist, handle);

  if (!u1 || (u1 && !whois_access(dcc[idx].user, u1))) {
    dprintf(idx, "No such user!\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# comment %s %s", dcc[idx].nick, handle, par);
  if (!strcasecmp(par, "none")) {
    dprintf(idx, "Okay, comment blanked.\n");
    set_user(&USERENTRY_COMMENT, u1, NULL);
    return;
  }
  dprintf(idx, "Changed comment.\n");
  update_mod(handle, dcc[idx].nick, "comment", NULL);
  set_user(&USERENTRY_COMMENT, u1, par);
  if (conf.bot->hub)
    write_userfile(idx);
}

static void cmd_randstring(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: randstring <len>\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# randstring %s", dcc[idx].nick, par);

  size_t len = atoi(par);

  if (len < 301) {
    char *randstring = NULL;

    randstring = (char *) calloc(1, len + 1);
    make_rand_str(randstring, len);
    dprintf(idx, "string: %s\n", randstring);
    free(randstring);
  } else 
    dprintf(idx, "Too long, must be <= 300\n");
}

static void cmd_hash(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: hash <string>\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# hash ...", dcc[idx].nick);
  dprintf(idx, "MD5(%s) = %s\n", par, MD5(par));
  dprintf(idx, "SHA1(%s) = %s\n", par, SHA1(par));
  char *salted = salted_sha1(par);
  dprintf(idx, "SALTED-SHA1(%s) = %s\n", par, salted);
  free(salted);
  dprintf(idx, "SHA256(%s) = %s\n", par, SHA256(par));
}

static void cmd_md5(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: md5 <string>\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# md5 ...", dcc[idx].nick);
  dprintf(idx, "MD5(%s) = %s\n", par, MD5(par));
}

static void cmd_sha1(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: sha1 <string>\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# sha1 ...", dcc[idx].nick);
  dprintf(idx, "SHA1(%s) = %s\n", par, SHA1(par));

  char *salted = salted_sha1(par);
  dprintf(idx, "SALTED-SHA1(%s) = %s\n", par, salted);
  free(salted);
}

static void cmd_sha256(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: sha256 <string>\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# sha256 ...", dcc[idx].nick);
  dprintf(idx, "SHA256(%s) = %s\n", par, SHA256(par));
}

static void cmd_conf(int idx, char *par)
{
  if (!conf.bot->localhub && !conf.bot->hub) {
    dprintf(idx, "Please use '%s%s%s' for this login/shell.\n", RED(idx), conf.localhub, COLOR_END(idx));
    return;
  }

  char *cmd = NULL;
  char *listbot = NULL;
  int save = 0;

  if (par[0])
    cmd = newsplit(&par);

  /* del/change should restart the specified bot ;) */

  if (!cmd) {
    if (!conf.bot->hub)
      dprintf(idx, "Usage: conf <add|del|change|disable|enable|list|set> [options]\n");
    else
      dprintf(idx, "Usage: conf <set> [options]\n");

    return;
  }

  conf_bot *oldlist = conf_bots_dup(conf.bots);
  
  putlog(LOG_CMDS, "*", "#%s# conf %s %s", dcc[idx].nick, cmd, par[0] ? par : "");
  if (!strcasecmp(cmd, "add") || !strcasecmp(cmd, "change")) {
    char *nick = NULL, *host = NULL, *ip = NULL, *ipsix = NULL;

    nick = newsplit(&par);
    if (!nick || (nick && !nick[0])) {
      dprintf(idx, "Usage: conf %s <bot> [<ip|.> <[+]host|.> [ipv6-ip]]\n", cmd);

      free_conf_bots(oldlist);
      return;
    }

    if (par[0])
      ip = newsplit(&par);
    if (par[0])
      host = newsplit(&par);
    if (par[0])
      ipsix = newsplit(&par);

    if (cmd[0] == 'c' || cmd[0] == 'C')
      conf_delbot(nick, 0);

    conf_addbot(nick, ip, host, ipsix);
    if (cmd[0] == 'a' || cmd[0] == 'A')
      dprintf(idx, "Added bot: %s\n", nick);
    else
      dprintf(idx, "Changed bot: %s\n", nick);
    listbot = strdup(nick);
    save++;
  } else if (!strncasecmp(cmd, "del", 3) || !strncasecmp(cmd, "rem", 3)) {
    if (!par[0]) {
      dprintf(idx, "Usage: conf del <bot>\n");

      free_conf_bots(oldlist);
      return;
    }

    if (!conf_delbot(par)) {
      struct userrec *u = NULL;

      dprintf(idx, "Deleted bot from conf: %s\n", par);
      if ((u = get_user_by_handle(userlist, par))) {
        if (!conf.bot->hub)
          check_this_user(par, 1, NULL);
        if (deluser(par)) {
          dprintf(idx, "Removed bot: %s.\n", par);
          if (conf.bot->hub)
            write_userfile(idx);
        }
      }

      save++;
    } else
      dprintf(idx, "Error trying to remove bot '%s'\n", par);
  } else if (!strcasecmp(cmd, "disable") || !strcasecmp(cmd, "enable")) {
    conf_bot *bot = NULL;

    for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
      if (!strcasecmp(bot->nick, par)) {
        if (!strcasecmp(cmd, "enable")) {
          if (bot->disabled) {
            dprintf(idx, "Enabled '%s'.\n", bot->nick);
            bot->disabled = 0;
            save++;
          } else
            dprintf(idx, "'%s' was already enabled!\n", bot->nick);            
        } else {
          if (!bot->disabled) {
            dprintf(idx, "Disabled '%s'.\n", bot->nick);
            bot->disabled = 1;
            save++;
          } else
            dprintf(idx, "'%s' was already disabled!\n", bot->nick);
        }
      }
    }
  }

  if (!strcasecmp(cmd, "set")) {
    char *what = NULL;
    int show = 1, set = 0;

    if (par[0]) {       
      what = newsplit(&par);

      if (par[0] && what) { /* set */
        set++;
        save = 1;
/*        if (!strcasecmp(what, "uid"))            conf.uid = atoi(par);
        else if (!strcasecmp(what, "username"))  str_redup(&conf.username, par);
*/
        if (!strcasecmp(what, "homedir"))   str_redup(&conf.homedir, par);
        else if (!strcasecmp(what, "datadir"))   str_redup(&conf.datadir, par);
        else if (!strcasecmp(what, "portmin"))   conf.portmin = atoi(par);
        else if (!strcasecmp(what, "portmax"))   conf.portmax = atoi(par);
        else if (!strcasecmp(what, "autocron"))  conf.autocron = atoi(par);
        else { 
          set--;
          save = 0;
          dprintf(idx, "Unknown option '%s'\n", par);
        }
      }
    }
    if (show) {
      const char *ss = set ? "Set: " : "";
      
/*      if (!what || !strcasecmp(what, "uid"))        dprintf(idx, "%suid: %d\n", ss, conf.uid);
      if (!what || !strcasecmp(what, "username"))   dprintf(idx, "%susername: %s\n", ss, conf.username);
*/
      if (!what || !strcasecmp(what, "homedir"))    dprintf(idx, "%shomedir: %s\n", ss, conf.homedir);
      if (!what || !strcasecmp(what, "datadir"))    dprintf(idx, "%sdatadir: %s\n", ss, conf.datadir);
      if (!what || !strcasecmp(what, "portmin"))    dprintf(idx, "%sportmin: %d\n", ss, conf.portmin);
      if (!what || !strcasecmp(what, "portmax"))    dprintf(idx, "%sportmax: %d\n", ss, conf.portmax);
      if (!what || !strcasecmp(what, "autocron"))   dprintf(idx, "%sautocron: %d\n", ss, conf.autocron);
    }
  }

  if (listbot || !strcasecmp(cmd, "list")) {
    conf_checkpids(conf.bots);
    conf_bot *bot = NULL;
    unsigned int i = 0;

    for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
      i++;
      if (!listbot || (listbot && !strcasecmp(listbot, bot->nick)))
        dprintf(idx, "%d: %s%s IP: %s HOST: %s IP6: %s HOST6: %s HUB: %d PID: %lu\n", i,
                      bot->disabled ? "/" : "",
                      bot->nick,
                      bot->net.ip ? bot->net.ip : "",
                      bot->net.host ? bot->net.host : "",
                      bot->net.ip6 ? bot->net.ip6 : "",
                      bot->net.host6 ? bot->net.host6 : "",
                      bot->hub,
                      (unsigned long) bot->pid);
    }
  }
  if (listbot)
    free(listbot);

  if (save) {
    /* rewrite our binary */
    conf_to_bin(&conf, 0, -1);

    /* Now signal all of the old bots with SIGUSR1
     * They will auto die and determine new localhub, etc..
     */
    conf_checkpids(oldlist);
    conf_killbot(oldlist, conf.bot->nick, NULL, SIGUSR1, 1); /* Don't kill me. */

    /* Now spawn new bots */
    conf_checkpids(conf.bots);
    spawnbots(conf.bots, 1); /* Not me. */
    conf_checkpids(conf.bots, 0);

    /* So reload_bin_data can process correctly */
    conf.bots = oldlist;

    do_restart = 2; /* SIGUSR1 -- reload_bin_data(); */
  } else
    free_conf_bots(oldlist);
}

static void cmd_encrypt(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: encrypt <key> <string>\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# encrypt ...", dcc[idx].nick);
  
  char *key = newsplit(&par);

  if (!par[0]) {
    dprintf(idx, "Usage: encrypt <key> <string>\n");
    return;
  }

  const char salt2[] = SALT2;
  bd::String crypted = encrypt_string(bd::String(key ? key : salt2), bd::String(par));
  crypted = bd::base64Encode(crypted);

  dprintf(idx, "encrypt(%s) = %s\n", par, crypted.c_str());
}

static void cmd_encrypt_fish(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: encrypt_fish <key> <string>\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# encrypt_fish ...", dcc[idx].nick);

  char *key = newsplit(&par);

  if (!par[0]) {
    dprintf(idx, "Usage: encrypt_fish <key> <string>\n");
    return;
  }

  const char salt2[] = SALT2;

  bd::String bf_crypt = egg_bf_encrypt(bd::String(par), bd::String(key ? key : salt2));
  dprintf(idx, "encrypt_fish(%s) = %s\n", par, bf_crypt.c_str());
}

static void cmd_decrypt_fish(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: decrypt_fish <key> <string>\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# decrypt_fish ...", dcc[idx].nick);

  char *key = newsplit(&par);

  if (!par[0]) {
    dprintf(idx, "Usage: decrypt_fish <key> <string>\n");
    return;
  }

  const char salt2[] = SALT2;

  bd::String bf_decrypt = egg_bf_decrypt(bd::String(par), bd::String(key ? key : salt2));
  dprintf(idx, "decrypt_fish(%s) = %s\n", par, bf_decrypt.c_str());
}

static void cmd_decrypt(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: decrypt <key> <string>\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# decrypt ...", dcc[idx].nick);
  
  char *key = newsplit(&par);

  if (!par[0]) {
    dprintf(idx, "Usage: decrypt <key> <string>\n");
    return;
  }

  const char salt2[] = SALT2;
  bd::String ciphertext = bd::base64Decode(par);
  bd::String cleartext = decrypt_string(bd::String(key ? key : salt2), ciphertext);

  dprintf(idx, "decrypt(%s) = %s\n", par, cleartext.c_str());
}

static void cmd_restart(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# restart", dcc[idx].nick);
  do_restart = 1;
}

static void cmd_reload(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# reload", dcc[idx].nick);
  dprintf(idx, "Reloading user file...\n");
  reload();
}

static void cmd_rehash(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# rehash", dcc[idx].nick);
  dprintf(idx, "Rehashing config data from binary...\n");
  do_restart = 2;
}

static void cmd_die(int idx, char *par)
{
  char s1[1024] = "", s2[1024] = "";

  putlog(LOG_CMDS, "*", "#%s# die %s", dcc[idx].nick, par);
  if (par[0]) {
    simple_snprintf(s1, sizeof s1, "BOT SHUTDOWN (%s: %s)", dcc[idx].nick, par);
    simple_snprintf(s2, sizeof s2, "DIE BY %s!%s (%s)", dcc[idx].nick, dcc[idx].host, par);
    strlcpy(quit_msg, par, 1024);
  } else {
    simple_snprintf(s1, sizeof s1, "BOT SHUTDOWN (Authorized by %s)", dcc[idx].nick);
    simple_snprintf(s2, sizeof s2, "DIE BY %s!%s (request)", dcc[idx].nick, dcc[idx].host);
    strlcpy(quit_msg, "requested", 1024);
  }
  kill_bot(s1, s2);
}

static void cmd_suicide(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# suicide %s", dcc[idx].nick, par);
  suicide(par);
}

static void cmd_debug(int idx, char *par)
{
  char *cmd = NULL;
  struct chanset_t* chan = NULL;
  size_t roleidx;
  bd::Array<bd::String> roles;

  if (!par[0]) 
    putlog(LOG_CMDS, "*", "#%s# debug", dcc[idx].nick);

  if (par[0])
    cmd = newsplit(&par);
  if (!cmd || (cmd && !strcmp(cmd, "timesync")))
    dprintf(idx, "Timesync: %li (%li)\n", (long) (now + timesync), (long)timesync);
  if (!cmd || (cmd && !strcmp(cmd, "now")))
    dprintf(idx, "Now: %li\n", (long)now);
  if (!cmd || (cmd && !strcmp(cmd, "role"))) {
    for (chan = chanset; chan; chan = chan->next) {
      if (chan->role) {
        roles.clear();
        for (roleidx = 0; role_counts[roleidx].name; ++roleidx) {
          if (chan->role & role_counts[roleidx].role) {
            roles << role_counts[roleidx].name;
          }
        }
        dprintf(idx, "Role: %-8s: %s\n", chan->dname,
            static_cast<bd::String>(roles.join(" ")).c_str());
      }
    }
  }
  if (!cmd || (cmd && !strcmp(cmd, "net")))
    tell_netdebug(idx);
  if (!cmd || (cmd && !strcmp(cmd, "dns")))
    tell_dnsdebug(idx);
}

static void cmd_timers(int idx, char *par)
{
  int *ids = 0, n = 0, called = 0;
  egg_timeval_t howlong, trigger_time, mynow, diff;

  if ((n = timer_list(&ids))) {
    int i = 0;
    char *name = NULL;

    timer_get_now(&mynow);

    for (i = 0; i < n; i++) {
      char interval[51] = "", next[51] = "";

      timer_info(ids[i], &name, &howlong, &trigger_time, &called);
      timer_diff(&mynow, &trigger_time, &diff);
      simple_snprintf(interval, sizeof interval, "(%lis %lims)", howlong.sec, howlong.usec / 1000);
      simple_snprintf(next, sizeof next, "%lis %lims", diff.sec, diff.usec / 1000);
      dprintf(idx, "%-2d: %-25s %-15s Next: %-25s Called: %d\n", i, name, interval, next, called);
    }
    free(ids);
  }
}

static void cmd_simul(int idx, char *par)
{
  char *nick = newsplit(&par);

  if (!par[0]) {
    dprintf(idx, "Usage: simul <hand> <text>\n");
    return;
  }


  struct userrec *u2 = get_user_by_handle(userlist, nick);

  if (!u2 || (u2 && !whois_access(dcc[idx].user, u2))) {
    dprintf(idx, "No such user.\n");
    return;
  }

  bool ok = 0;

  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type->flags & DCT_SIMUL) && !ok && !strcasecmp(nick, dcc[i].nick)) {
      putlog(LOG_CMDS, "*", "#%s# simul %s %s", dcc[idx].nick, nick, par);
      if (dcc[i].type && dcc[i].type->activity) {
	dcc[i].type->activity(i, par, strlen(par));
	ok = 1;
      }
    }
  }
  if (!ok)
    dprintf(idx, "No such user on the party line.\n");
}

static void cmd_link(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: link [some-bot] <new-bot>\n");
    return;
  }
  putlog(LOG_CMDS, "*", "#%s# link %s", dcc[idx].nick, par);

  char *s = newsplit(&par);
  char *tolink = par[0] ? par : s;
  struct userrec *u = get_user_by_handle(userlist, tolink);

  if (!u || bot_hublevel(u) == 999) {
    dprintf(idx, "You can only link to other hubs.\n");
    return;
  }

  if (!par[0] || !strcasecmp(par, conf.bot->nick))
    botlink(dcc[idx].nick, idx, s);
  else {
    char x[40] = "";
    int i = nextbot(s);

    if (i < 0) {
      dprintf(idx, "No such bot online.\n");
      return;
    }
    simple_snprintf(x, sizeof(x), "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, conf.bot->nick);
    botnet_send_link(i, x, s, par);
  }
}

static void cmd_unlink(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: unlink <bot> [reason]\n");
    return;
  }
  putlog(LOG_CMDS, "*", "#%s# unlink %s", dcc[idx].nick, par);

  char *bot = newsplit(&par);
  int i = nextbot(bot);

  if (i < 0) {
    botunlink(idx, bot, par);
    return;
  }
  /* If we're directly connected to that bot, just do it
   * (is nike gunna sue?)
   */
  if (!strcasecmp(dcc[i].nick, bot))
    botunlink(idx, bot, par);
  else {
    char x[40] = "";

    simple_snprintf(x, sizeof(x), "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, conf.bot->nick);
    botnet_send_unlink(i, x, lastbot(bot), bot, par);
  }
}

static void cmd_relay(int idx, char *par)
{
  if (dcc[idx].simul >= 0) {
    dprintf(idx, "Sorry, that cmd isn't available over botcmd.\n");
    return;
  }
  if (!par[0]) {
    dprintf(idx, "Usage: relay <bot>\n");
    return;
  }
  putlog(LOG_CMDS, "*", "#%s# relay %s", dcc[idx].nick, par);
  tandem_relay(idx, par, 0);
}

static void cmd_save(int idx, char *par)
{
  char buf[100] = "";

  putlog(LOG_CMDS, "*", "#%s# save", dcc[idx].nick);
  simple_snprintf(buf, sizeof(buf), "Saving user file...");
  int i = write_userfile(-1);
  if (i == 0)
    strlcat(buf, "success.", sizeof(buf));
  else if (i == 1)
    strlcat(buf, "failed: No userlist.", sizeof(buf));
  else if (i == 2)
    strlcat(buf, "failed: Cannot open userfile for writing.", sizeof(buf));
  else if (i == 3)
    strlcat(buf, "failed: Problem writing users/chans (see debug).", sizeof(buf));
  else		/* This can't happen. */
    strlcat(buf, "failed: Unforseen error", sizeof(buf));

  dprintf(idx, "%s\n", buf);
}

static void cmd_backup(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# backup", dcc[idx].nick);
  dprintf(idx, "Backing up the channel & user files...\n");
  write_userfile(idx);
  backup_userfile();
}

static void cmd_trace(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: trace <bot>\n");
    return;
  }
  if (!strcasecmp(par, conf.bot->nick)) {
    dprintf(idx, "That's me!  Hiya! :)\n");
    return;
  }

  int i = nextbot(par);

  if (i < 0) {
    dprintf(idx, "Unreachable bot.\n");
    return;
  }

  char x[NOTENAMELEN + 11] = "", y[11] = "";

  putlog(LOG_CMDS, "*", "#%s# trace %s", dcc[idx].nick, par);
  simple_snprintf(x, sizeof(x), "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, conf.bot->nick);

  egg_timeval_t tv;

  timer_get_now(&tv);

  simple_snprintf(y, sizeof(y), ":%li", (long) ((tv.sec % 10000) * 100 + (tv.usec * 100) / (1000000)));
  botnet_send_trace(i, x, par, y);
}

/* After messing with someone's user flags, make sure the dcc-chat flags
 * are set correctly.
 */
int check_dcc_attrs(struct userrec *u, flag_t oatr)
{
  if (!u)
    return 0;

  /* Make sure default owners are +a */
  if (isowner(u->handle)) {
    u->flags = sanity_check(u->flags | USER_ADMIN, 0);
  }

  int stat;

  for (int i = 0; i < dcc_total; i++) {
   if (dcc[i].type && dcc[i].simul == -1) {
    if (dcc[i].type == &DCC_CHAT && !conf.bot->hub && !ischanhub() && u == conf.bot->u) {
      dprintf(i, "I am no longer a chathub..\n");
      do_boot(i, conf.bot->nick, "I am no longer a chathub.");
      continue;
    }

    if ((dcc[i].type->flags & DCT_MASTER) && (!strcasecmp(u->handle, dcc[i].nick))) {
      stat = dcc[i].status;
      if ((dcc[i].type == &DCC_CHAT) &&
	  ((u->flags & (USER_OP | USER_MASTER | USER_OWNER))
	   != (oatr & (USER_OP | USER_MASTER | USER_OWNER)))) {
	botnet_send_join_idx(i);
      }
      if ((oatr & USER_MASTER) && !(u->flags & USER_MASTER)) {
	struct flag_record fr = {FR_CHAN | FR_ANYWH, 0, 0, 0 };

	dcc[i].u.chat->con_flags &= ~(LOG_MISC | LOG_CMDS | LOG_RAW |
				      LOG_FILES | LOG_WALL | LOG_DEBUG);
	get_user_flagrec(u, &fr, NULL);
	if (!chan_master(fr))
	  dcc[i].u.chat->con_flags |= (LOG_MISC | LOG_CMDS);
	dprintf(i, "*** POOF! ***\n");
	dprintf(i, "You are no longer a master on this bot.\n");
      }
      if (!(oatr & USER_MASTER) && (u->flags & USER_MASTER)) {
	dcc[i].u.chat->con_flags |= conmask;
	dprintf(i, "*** POOF! ***\n");
	dprintf(i, "You are now a master on this bot.\n");
      }
      if (!(oatr & USER_PARTY) && (u->flags & USER_PARTY) && dcc[i].u.chat->channel < 0) {
        dprintf(i, "-+- POOF! -+-\n");
        dprintf(i, "You now have party line chat access.\n");
        dprintf(i, "To rejoin the partyline, type: %schat on\n", (dcc[i].u.chat->channel >= 0) ? settings.dcc_prefix : "");
      }

      if (!(oatr & USER_OWNER) && (u->flags & USER_OWNER)) {
	dprintf(i, "@@@ POOF! @@@\n");
	dprintf(i, "You are now an OWNER of this bot.\n");
      }
      if ((oatr & USER_OWNER) && !(u->flags & USER_OWNER)) {
	dprintf(i, "@@@ POOF! @@@\n");
	dprintf(i, "You are no longer an owner of this bot.\n");
      }

      if (!(u->flags & USER_PARTY) && dcc[i].u.chat->channel >= 0) { /* who cares about old flags, they shouldnt be here anyway. */
        dprintf(i, "-+- POOF! -+-\n");
        dprintf(i, "You no longer have party line chat access.\n");
        dprintf(i, "Leaving chat mode...\n");
        chanout_but(-1, dcc[i].u.chat->channel, "*** %s left the party line - no chat access.\n", dcc[i].nick);
        if (dcc[i].u.chat->channel < 100000)
          botnet_send_part_idx(i, "");
        dcc[i].u.chat->channel = (-1);
      }

      if (!(oatr & USER_ADMIN) && (u->flags & USER_ADMIN)) {
	dprintf(i, "^^^ POOF! ^^^\n");
	dprintf(i, "You are now an ADMIN of this bot.\n");
      }
      if ((oatr & USER_ADMIN) && !(u->flags & USER_ADMIN)) {
	dprintf(i, "^^^ POOF! ^^^\n");
	dprintf(i, "You are no longer an admin of this bot.\n");
      }
      if ((stat & STAT_PARTY) && (u->flags & USER_OP))
	stat &= ~STAT_PARTY;
      if (!(stat & STAT_PARTY) && !(u->flags & USER_OP) &&
	  !(u->flags & USER_MASTER))
	stat |= STAT_PARTY;
      if (stat & STAT_CHAT) {
       if (conf.bot->hub && !(u->flags & USER_HUBA))        
         stat &= ~STAT_CHAT;
       if (ischanhub() && !(u->flags & USER_CHUBA))
         stat &= ~STAT_CHAT;
      }
      if ((u->flags & USER_PARTY))
	stat |= STAT_CHAT;
      dcc[i].status = stat;
      /* Check if they no longer have access to wherever they are.
       */
      if (conf.bot->hub && !(u->flags & (USER_HUBA))) {
        /* no hub access, drop them. */
        dprintf(i, "-+- POOF! -+-\n");
        dprintf(i, "You no longer have hub access.\n");
        do_boot(i, conf.bot->nick, "No hub access.");
        continue;
      }     
      if (!conf.bot->hub && ischanhub() && !(u->flags & (USER_CHUBA))) {
        /* no chanhub access, drop them. */
        dprintf(i, "-+- POOF! -+-\n");
        dprintf(i, "You no longer have chathub access.\n");
        do_boot(i, conf.bot->nick, "No chathub access.");
        continue;
      }
    }
   }
  }
  return u->flags;
}

int check_dcc_chanattrs(struct userrec *u, char *chname, flag_t chflags, flag_t ochatr)
{
  if (!u)
    return 0;

  int found = 0, atr = u ? u->flags : 0;
  struct chanset_t *chan = NULL;
  struct chat_info dummy;

  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].simul == -1 && (dcc[i].type->flags & DCT_MASTER) && !strcasecmp(u->handle, dcc[i].nick)) {
      if ((dcc[i].type == &DCC_CHAT) &&
	  ((chflags & (USER_OP | USER_MASTER | USER_OWNER))
	   != (ochatr & (USER_OP | USER_MASTER | USER_OWNER))))
	botnet_send_join_idx(i);
      if ((ochatr & USER_MASTER) && !(chflags & USER_MASTER)) {
	if (!(atr & USER_MASTER))
	  dcc[i].u.chat->con_flags &= ~(LOG_MISC | LOG_CMDS);
	dprintf(i, "*** POOF! ***\n");
	dprintf(i, "You are no longer a master on %s.\n", chname);
      }
      if (!(ochatr & USER_MASTER) && (chflags & USER_MASTER)) {
	dcc[i].u.chat->con_flags |= conmask;
	if (!(atr & USER_MASTER))
	  dcc[i].u.chat->con_flags &=
	    ~(LOG_RAW | LOG_DEBUG | LOG_WALL | LOG_FILES | LOG_SRVOUT);
	dprintf(i, "*** POOF! ***\n");
	dprintf(i, "You are now a master on %s.\n", chname);
      }
      if (!(ochatr & USER_OWNER) && (chflags & USER_OWNER)) {
	dprintf(i, "@@@ POOF! @@@\n");
	dprintf(i, "You are now an OWNER of %s.\n", chname);
      }
      if ((ochatr & USER_OWNER) && !(chflags & USER_OWNER)) {
	dprintf(i, "@@@ POOF! @@@\n");
	dprintf(i, "You are no longer an owner of %s.\n", chname);
      }
      if (((ochatr & (USER_OP | USER_MASTER | USER_OWNER)) &&
	   (!(chflags & (USER_OP | USER_MASTER | USER_OWNER)))) ||
	  ((chflags & (USER_OP | USER_MASTER | USER_OWNER)) &&
	   (!(ochatr & (USER_OP | USER_MASTER | USER_OWNER))))) {
	struct flag_record fr = {FR_CHAN, 0, 0, 0 };

	for (chan = chanset; chan && !found; chan = chan->next) {
	  get_user_flagrec(u, &fr, chan->dname);
	  if (fr.chan & (USER_OP | USER_MASTER | USER_OWNER))
	    found = 1;
	}
	if (!chan)
	  chan = chanset;
	if (chan)
	  strlcpy(dcc[i].u.chat->con_chan, chan->dname, sizeof(dummy.con_chan));
	else
	  strlcpy(dcc[i].u.chat->con_chan, "*", 2);
      }
    }
  }
  return chflags;
}

static void cmd_chattr(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: chattr <handle> [changes] [channel]\n");
    return;
  }

  char *hand = newsplit(&par);
  struct userrec *u2 = get_user_by_handle(userlist, hand);

  if (!u2 || (u2 && !whois_access(dcc[idx].user, u2))) {
    dprintf(idx, "No such user!\n");
    return;
  }

  char *arg = NULL, *tmpchg = NULL, *chg = NULL, work[1024] = "";
  struct chanset_t *chan = NULL;
  struct flag_record pls = {0, 0, 0, 0 },
  		     mns = {0, 0, 0, 0 },
		     user = {0, 0, 0, 0 },
		     ouser = {0, 0, 0, 0 };
  flag_t of = 0, ocf = 0;
  bool save = 0;

  /* Parse args */
  if (par[0]) {
    arg = newsplit(&par);
    if (par[0]) {
      /* .chattr <handle> <changes> <channel> */
      chg = arg;
      arg = newsplit(&par);
      chan = findchan_by_dname(arg);
    } else {
      chan = findchan_by_dname(arg);
      /* Consider modeless channels, starting with '+' */
      if (!(arg[0] == '+' && chan) &&
          !(arg[0] != '+' && strchr (CHANMETA, arg[0]))) {
	/* .chattr <handle> <changes> */
        chg = arg;
        chan = NULL; /* uh, !strchr (CHANMETA, channel[0]) && channel found?? */
	arg = NULL;
      }
      /* .chattr <handle> <channel>: nothing to do... */
    }
  }
  /* arg:  pointer to channel name, NULL if none specified
   * chan: pointer to channel structure, NULL if none found or none specified
   * chg:  pointer to changes, NULL if none specified
   */
  if (arg && !chan) {
    dprintf(idx, "No channel record for %s.\n", arg);
    return;
  }
  if (chg) {
    if (!arg && strpbrk(chg, "&|")) {
      /* .chattr <handle> *[&|]*: use console channel if found... */
      if (!strcmp ((arg = dcc[idx].u.chat->con_chan), "*"))
        arg = NULL;
      else
        chan = findchan_by_dname(arg);
      if (arg && !chan) {
        dprintf (idx, "Invalid console channel %s.\n", arg);
	return;
      }
    } else if (arg && !strpbrk(chg, "&|")) {
      size_t tmpsiz = strlen(chg) + 2;
      tmpchg = (char *) calloc(1, tmpsiz);
      strlcpy(tmpchg, "|", 2);
      strlcat(tmpchg, chg, tmpsiz);
      chg = tmpchg;
    }
  }
  par = arg;
  user.match = FR_GLOBAL;
  if (chan)
    user.match |= FR_CHAN;
  if (u2->bot)
    user.match |= FR_BOT;

  get_user_flagrec(dcc[idx].user, &user, chan ? chan->dname : 0);
  get_user_flagrec(u2, &ouser, chan ? chan->dname : 0);
  if (!chan && !glob_master(user)) {
    dprintf(idx, "You do not have global master access.\n");
    if (tmpchg)
      free(tmpchg);
    return;
  }

  if (chan && !glob_master(user) && !chan_master(user)) {
    dprintf(idx, "You do not have channel master privileges for channel %s.\n",
	    par);
    if (tmpchg)
      free(tmpchg);
    return;
  }
    if (chan && (privchan(user, chan, PRIV_OP) || (channel_privchan(chan) && !(chan_master(user) || glob_owner(user))))) {
      dprintf(idx, "You do not have access to change flags for %s\n", chan->dname);
      if (tmpchg)
        free(tmpchg);
      return;
    }

  if (chg) {
    int okp = 1;

    pls.match = user.match;
    break_down_flags(chg, &pls, &mns);

    if ((pls.global & BOT_UPDATEHUB) && (bot_hublevel(u2) == 999)) {
      dprintf(idx, "Only a hub can be set as the updatehub.\n");
      pls.global &= ~(BOT_UPDATEHUB);
    }
    
    /* strip out +p without +i or +j */
    if ((pls.global & (USER_CHUBA | USER_HUBA)) || glob_huba(ouser) || glob_chuba(ouser))
      okp = 1;
    
    if ((pls.global & USER_PARTY) && !okp) {
      dprintf(idx, "Global flag +p requires either chathub or hub access first.\n");
      pls.global &= ~USER_PARTY;
    }
    
    if (!isowner(dcc[idx].nick)) {
      if (pls.global & USER_HUBA)
        putlog(LOG_MISC, "*", "%s attempted to give %s hub connect access", dcc[idx].nick, u2->handle);
      if (mns.global & USER_HUBA)
        putlog(LOG_MISC, "*", "%s attempted to take away hub connect access from %s", dcc[idx].nick, u2->handle);
      if (pls.global & BOT_UPDATEHUB)
        putlog(LOG_MISC, "*", "%s attempted to make %s the updatehub", dcc[idx].nick, u2->handle);
      if (mns.global & BOT_UPDATEHUB)
        putlog(LOG_MISC, "*", "%s attempted to take away updatehub status from %s", dcc[idx].nick, u2->handle);
      if (pls.global & USER_ADMIN)
        putlog(LOG_MISC, "*", "%s attempted to give %s admin access", dcc[idx].nick, u2->handle);
      if (mns.global & USER_ADMIN)
        putlog(LOG_MISC, "*", "%s attempted to take away admin access from %s", dcc[idx].nick, u2->handle);
      if (pls.global & USER_OWNER)
        putlog(LOG_MISC, "*", "%s attempted to give owner to %s", dcc[idx].nick, u2->handle);
      if (mns.global & USER_OWNER)
        putlog(LOG_MISC, "*", "%s attempted to take owner away from %s", dcc[idx].nick, u2->handle);
      pls.global &=~(USER_HUBA | USER_ADMIN | USER_OWNER | BOT_UPDATEHUB);
      mns.global &=~(USER_HUBA | USER_ADMIN | USER_OWNER | BOT_UPDATEHUB);
      pls.chan &= ~(USER_ADMIN | BOT_UPDATEHUB);
    }
    if (chan) {
      pls.chan &= ~(USER_HUBA | USER_CHUBA);
      mns.chan &= ~(USER_HUBA | USER_CHUBA);
    }
    if (!glob_owner(user) && !isowner(dcc[idx].nick)) {
      pls.global &= ~(USER_CHUBA | USER_OWNER | USER_MASTER);
      mns.global &= ~(USER_CHUBA | USER_OWNER | USER_MASTER);

      if (chan) {
	pls.chan &= ~USER_OWNER;
	mns.chan &= ~USER_OWNER;
      }
    }
    if (chan && !chan_owner(user) && !glob_owner(user) && !isowner(dcc[idx].nick)) {
      pls.chan &= ~USER_MASTER;
      mns.chan &= ~USER_MASTER;
      if (!chan_master(user) && !glob_master(user)) {
	pls.chan = 0;
	mns.chan = 0;
      }
    }
    if (!conf.bot->hub) {
      pls.global &=~(USER_OWNER | USER_ADMIN | USER_HUBA | USER_CHUBA);
      mns.global &=~(USER_OWNER | USER_ADMIN | USER_HUBA | USER_CHUBA);
    }
    get_user_flagrec(u2, &user, par);
    if (user.match & FR_GLOBAL) {
      of = user.global;
      user.global = sanity_check((user.global |pls.global) &~mns.global, u2->bot);
    }
    if (chan) {
      ocf = user.chan;
      user.chan = chan_sanity_check((user.chan | pls.chan) & ~mns.chan, u2->bot);
    }
    set_user_flagrec(u2, &user, par);
    save = 1;
  }
  if (save) {
    if (chan) {
      char tmp[100] = "";

      putlog(LOG_CMDS, "*", "#%s# (%s) chattr %s %s", dcc[idx].nick, chan ? chan->dname : "*", hand, chg ? chg : "");
      simple_snprintf(tmp, sizeof tmp, "chattr %s", chg);
      update_mod(hand, dcc[idx].nick, tmp, chan->dname);
    } else {
      putlog(LOG_CMDS, "*", "#%s# chattr %s %s", dcc[idx].nick, hand, chg ? chg : "");
      update_mod(hand, dcc[idx].nick, "chattr", chg);
    }
    if (chg && (pls.global & USER_ADMIN)) { // +a was used, warn that this is admin and not auto-op.
      dprintf(idx, "WARNING: You have just used +a, which is an ADMIN privilege.\n");
      dprintf(idx, "If you intended to use the AUTO-OP privilege, please use +O instead.\n");
    }
  }
  /* Get current flags and display them */

  if (whois_access(dcc[idx].user, u2)) {
    if (user.match & FR_GLOBAL) {
      user.match = FR_GLOBAL;
      if (chg)
        check_dcc_attrs(u2, of);
      get_user_flagrec(u2, &user, NULL);
      build_flags(work, &user, NULL);
      if (work[0] != '-')
        dprintf(idx, "Global flags for %s are now +%s.\n", hand, work);
      else
        dprintf(idx, "No global flags for %s.\n", hand);
    }
    if (chan) {
      user.match = FR_CHAN;
      get_user_flagrec(u2, &user, par);
      if (chg)
        check_dcc_chanattrs(u2, chan->dname, user.chan, ocf);
      build_flags(work, &user, NULL);
      if (work[0] != '-')
        dprintf(idx, "Channel flags for %s on %s are now +%s.\n", hand, chan->dname, work);
      else {
        dprintf(idx, "No flags for %s on %s.\n", hand, chan->dname);
        del_chanrec(u2, chan->dname);
      }
    }
  }
  if (chg && !conf.bot->hub)
    check_this_user(hand, 0, NULL);
  if (tmpchg)
    free(tmpchg);
  if (conf.bot->hub && save)
    write_userfile(idx);
}

static void cmd_chat(int idx, char *par)
{
  if (!(dcc[idx].user->flags & USER_PARTY)) {
    dprintf(idx, "You don't have partyline access\n");
    return;
  }

  char *arg = newsplit(&par);

  if (!strcasecmp(arg, "off")) {
    /* Turn chat off */
    if (dcc[idx].u.chat->channel < 0) {
      dprintf(idx, "You weren't in chat anyway!\n");
      return;
    } else {
      dprintf(idx, "Leaving chat mode...\n");
      chanout_but(-1, dcc[idx].u.chat->channel,
		  "*** %s left the party line.\n",
		  dcc[idx].nick);
      if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
	botnet_send_part_idx(idx, "");
    }
    dcc[idx].u.chat->channel = (-1);
  } else {
    int newchan, oldchan;

    if (arg[0] == '*') {
      if (((arg[1] < '0') || (arg[1] > '9'))) {
	if (!arg[1])
	  newchan = 0;
	else {
          newchan = -1;
	}
	if (newchan < 0) {
	  dprintf(idx, "No channel exists by that name.\n");
	  return;
	}
      } else
	newchan = GLOBAL_CHANS + atoi(arg + 1);
      if (newchan < GLOBAL_CHANS || newchan > 199999) {
	dprintf(idx, "Channel number out of range: local channels must be *0-*99999.\n");
	return;
      }
    } else {
      if (((arg[0] < '0') || (arg[0] > '9')) && (arg[0])) {
	if (!strcasecmp(arg, "on"))
	  newchan = 0;
	else {
          newchan = -1;
	}
	if (newchan < 0) {
	  dprintf(idx, "No channel exists by that name.\n");
	  return;
	}
      } else
	newchan = atoi(arg);
      if ((newchan < 0) || (newchan > 99999)) {
	dprintf(idx, "Channel number out of range: must be between 0 and 99999.\n");
	return;
      }
    }
    /* If coming back from being off the party line, make sure they're
     * not away.
    */
    if ((dcc[idx].u.chat->channel < 0) && (dcc[idx].u.chat->away != NULL))
      not_away(idx);
    if (dcc[idx].u.chat->channel == newchan) {
      if (!newchan) {
	dprintf(idx, "You're already on the party line!\n");
        return;
      } else {
	dprintf(idx, "You're already on channel %s%d!\n",
		(newchan < GLOBAL_CHANS) ? "" : "*", newchan % GLOBAL_CHANS);
        return;
      }
    } else {
      oldchan = dcc[idx].u.chat->channel;
      if (!oldchan) {
	chanout_but(-1, 0, "*** %s left the party line.\n", dcc[idx].nick);
      } else if (oldchan > 0) {
	chanout_but(-1, oldchan, "*** %s left the channel.\n", dcc[idx].nick);
      }
      dcc[idx].u.chat->channel = newchan;
      if (!newchan) {
	dprintf(idx, "Entering the party line...\n");
	chanout_but(-1, 0, "*** %s joined the party line.\n", dcc[idx].nick);
      } else {
	dprintf(idx, "Joining channel '%s'...\n", arg);
	chanout_but(-1, newchan, "*** %s joined the channel.\n", dcc[idx].nick);
      }
      if (newchan < GLOBAL_CHANS)
	botnet_send_join_idx(idx);
      else if (oldchan < GLOBAL_CHANS)
	botnet_send_part_idx(idx, "");
    }
  }
  console_dostore(idx);
}

int exec_str(int idx, char *cmd) {
  char *out = NULL, *err = NULL;

  if (shell_exec(cmd, NULL, &out, &err)) {
    char *p = NULL, *np = NULL;

    if (out) {
      dprintf(idx, "Result:\n");
      p = out;
      while (p && p[0]) {
        np = strchr(p, '\n');
        if (np)
          *np++ = 0;
        dprintf(idx, "%s\n", p);
        p = np;
      }
      dprintf(idx, "\n");
      free(out);
    }
    if (err) {
      dprintf(idx, "Errors:\n");
      p = err;
      while (p && p[0]) {
        np = strchr(p, '\n');
        if (np)
          *np++ = 0;
        dprintf(idx, "%s\n", p);
        p = np;
      }
      dprintf(idx, "\n");
      free(err);
    }
    return 1;
  }
  return 0;
}

static void cmd_exec(int idx, char *par) {
  putlog(LOG_CMDS, "*", "#%s# exec %s", dcc[idx].nick, par);

  if (!conf.bot->hub && !isowner(dcc[idx].nick)) {
    putlog(LOG_WARN, "*", "%s attempted 'exec' %s", dcc[idx].nick, par);
    dprintf(idx, "exec is only available to permanent owners on leaf bots\n");
    return;
  }
  if (exec_str(idx, par))
    dprintf(idx, "Exec completed\n");
  else
    dprintf(idx, "Exec failed\n");
}

static void cmd_w(int idx, char *par) {
  putlog(LOG_CMDS, "*", "#%s# w", dcc[idx].nick);

  if (!exec_str(idx, "w"))
    dprintf(idx, "Exec failed\n");
}

static void cmd_ps(int idx, char *par) {
  putlog(LOG_CMDS, "*", "#%s# ps %s", dcc[idx].nick, par);

  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>') || strchr(par, '`') || strchr(par, '$')) {
    putlog(LOG_WARN, "*", "%s attempted 'ps' with pipe/semicolon in parameters: %s", dcc[idx].nick, par);
    dprintf(idx, "No.\n");
    return;
  }

  size_t size = strlen(par) + 9 + 1;
  char *buf = (char *) calloc(1, size);

  simple_snprintf(buf, size, "ps %s", par);
  if (!exec_str(idx, buf))
    dprintf(idx, "Exec failed\n");
  free(buf);
}

static void cmd_last(int idx, char *par) {
  putlog(LOG_CMDS, "*", "#%s# last %s", dcc[idx].nick, par);
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>') || strchr(par, '`') || strchr(par, '$')) {
    putlog(LOG_WARN, "*", "%s attempted 'last' with pipe/semicolon in parameters: %s", dcc[idx].nick, par);
    dprintf(idx, "No.\n");
    return;
  }

  char user[20] = "";

  if (par[0]) {
    strlcpy(user, par, sizeof(user));
  } else if (conf.username) {
    strlcpy(user, conf.username, sizeof(user));
  }
  if (!user[0]) {
    dprintf(idx, "Can't determine user id for process\n");
    return;
  }
  
  char buf[30] = "";

  simple_snprintf(buf, sizeof buf, "last %s", user);
  if (!exec_str(idx, buf))
    dprintf(idx, "Failed to execute /bin/sh last\n");
}


static void cmd_echo(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Echo is currently %s.\n", dcc[idx].status & STAT_ECHO ? "on" : "off");
    return;
  }
  if (!strcasecmp(par, "on")) {
    dprintf(idx, "Echo turned on.\n");
    dcc[idx].status |= STAT_ECHO;
  } else if (!strcasecmp(par, "off")) {
    dprintf(idx, "Echo turned off.\n");
    dcc[idx].status &= ~STAT_ECHO;
  } else {
    dprintf(idx, "Usage: echo <on/off>\n");
    return;
  }
  console_dostore(idx);
}

static void cmd_login(int idx, char *par)
{
  char *which = NULL;
  int set = -1, whichbit = 0;

  if (!par[0]) {
    dprintf(idx, "Usage: login <banner|bots|channels|whom> [on/off]\n");
    return;
  }

  which = newsplit(&par);

  if (!strcasecmp(which, "banner"))
    whichbit = STAT_BANNER;
  else if (!strcasecmp(which, "bots"))
    whichbit = STAT_BOTS;
  else if (!strcasecmp(which, "channels"))
    whichbit = STAT_CHANNELS;
  else if (!strcasecmp(which, "whom"))
    whichbit = STAT_WHOM;
  else {
    dprintf(idx, "Unrecognized option '$b%s$b'\n", which);
    return;
  }

  if (!par[0]) {
    dprintf(idx, "'%s' is currently: $b%s$b\n", which, (dcc[idx].status & whichbit) ? "on" : "off");
    return;
  }

  if (!strcasecmp(par, "on"))
    set = 1;
  else if (!strcasecmp(par, "off"))
    set = 0;
  else {
    dprintf(idx, "Unrecognized setting '$b%s$b'\n", par);
    return;
  }

  if (set)
    dcc[idx].status |= whichbit;
  else if (!set)
    dcc[idx].status &= ~whichbit;
  
  console_dostore(idx);
}

static void cmd_color(int idx, char *par)
{
  int ansi = coloridx(idx);

  putlog(LOG_CMDS, "*", "#%s# color %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: color <on/off>\n");
    if (ansi)
      dprintf(idx, "Color is currently on. (%s)\n", ansi == 1 ? "ANSI" : "mIRC");
    else
      dprintf(idx, "Color is currently off.\n");
    return;
  }

  char *of = newsplit(&par);

  if (!strcasecmp(of, "on")) {
    dcc[idx].status |= STAT_COLOR;
  } else if (!strcasecmp(of, "off")) {
    dcc[idx].status &= ~(STAT_COLOR);
    dprintf(idx, "Color turned off.\n");
  } else {
    return;
  }
  console_dostore(idx);
}

int stripmodes(const char *s)
{
  int res = 0;

  for (; *s; s++)
    switch (tolower(*s)) {
    case 'b':
      res |= STRIP_BOLD;
      break;
    case 'c':
      res |= STRIP_COLOR;
      break;
    case 'r':
      res |= STRIP_REV;
      break;
    case 'u':
      res |= STRIP_UNDER;
      break;
    case 'a':
      res |= STRIP_ANSI;
      break;
    case 'g':
      res |= STRIP_BELLS;
      break;
    case '*':
      res |= STRIP_ALL;
      break;
    }
  return res;
}

const char *stripmasktype(int x)
{
  static char s[20] = "";
  char *p = s;

  if (x & STRIP_BOLD)
    *p++ = 'b';
  if (x & STRIP_COLOR)
    *p++ = 'c';
  if (x & STRIP_REV)
    *p++ = 'r';
  if (x & STRIP_UNDER)
    *p++ = 'u';
  if (x & STRIP_ANSI)
    *p++ = 'a';
  if (x & STRIP_BELLS)
    *p++ = 'g';
  if (p == s)
    *p++ = '-';
  *p = 0;
  return s;
}

static const char *stripmaskname(int x)
{
  static char s[161] = "";
  size_t i = 0;

  s[0] = 0;
  if (x & STRIP_BOLD)
    i += my_strcpy(s + i, "bold, ");
  if (x & STRIP_COLOR)
    i += my_strcpy(s + i, "color, ");
  if (x & STRIP_REV)
    i += my_strcpy(s + i, "reverse, ");
  if (x & STRIP_UNDER)
    i += my_strcpy(s + i, "underline, ");
  if (x & STRIP_ANSI)
    i += my_strcpy(s + i, "ansi, ");
  if (x & STRIP_BELLS)
    i += my_strcpy(s + i, "bells, ");
  if (!i)
    strlcpy(s, "none", sizeof(s));
  else
    s[i - 2] = 0;
  return s;
}

static void cmd_strip(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Your current strip settings are: %s (%s).\n",
	    stripmasktype(dcc[idx].u.chat->strip_flags),
	    stripmaskname(dcc[idx].u.chat->strip_flags));
    return;
  }

  char *nick = newsplit(&par), *changes = NULL, *c = NULL, s[2] = "";
  int dest = 0, i, pls, md, ok = 0;

  if ((nick[0] != '+') && (nick[0] != '-') &&dcc[idx].user && (dcc[idx].user->flags & USER_MASTER)) {
    for (i = 0; i < dcc_total; i++) {
      if (dcc[i].type && !strcasecmp(nick, dcc[i].nick) && dcc[i].type == &DCC_CHAT && !ok) {
	ok = 1;
	dest = i;
      }
    }
    if (!ok) {
      dprintf(idx, "No such user on the party line!\n");
      return;
    }
    changes = par;
  } else {
    changes = nick;
    nick = "";
    dest = idx;
  }
  c = changes;
  if ((c[0] != '+') && (c[0] != '-'))
    dcc[dest].u.chat->strip_flags = 0;
  s[1] = 0;
  for (pls = 1; *c; c++) {
    switch (*c) {
    case '+':
      pls = 1;
      break;
    case '-':
      pls = 0;
      break;
    default:
      s[0] = *c;
      md = stripmodes(s);
      if (pls == 1)
	dcc[dest].u.chat->strip_flags |= md;
      else
	dcc[dest].u.chat->strip_flags &= ~md;
    }
  }
  if (nick[0])
    putlog(LOG_CMDS, "*", "#%s# strip %s %s", dcc[idx].nick, nick, changes);
  else
    putlog(LOG_CMDS, "*", "#%s# strip %s", dcc[idx].nick, changes);
  if (dest == idx) {
    dprintf(idx, "Your strip settings are: %s (%s).\n",
	    stripmasktype(dcc[idx].u.chat->strip_flags),
	    stripmaskname(dcc[idx].u.chat->strip_flags));
  } else {
    dprintf(idx, "Strip setting for %s: %s (%s).\n", dcc[dest].nick,
	    stripmasktype(dcc[dest].u.chat->strip_flags),
	    stripmaskname(dcc[dest].u.chat->strip_flags));
    dprintf(dest, "%s set your strip settings to: %s (%s).\n", dcc[idx].nick,
	    stripmasktype(dcc[dest].u.chat->strip_flags),
	    stripmaskname(dcc[dest].u.chat->strip_flags));
  }
  /* Set highlight flag here so user is able to control stripping of
   * bold also as intended -- dw 27/12/1999
   */
  console_dostore(dest);
}

static void cmd_su(int idx, char *par)
{
  if (dcc[idx].simul >= 0) {
    dprintf(idx, "Sorry, that cmd isn't available over botcmd.\n");
    return;
  }

  int atr = dcc[idx].user ? dcc[idx].user->flags : 0, ok;
  struct flag_record fr = {FR_ANYWH | FR_CHAN | FR_GLOBAL, 0, 0, 0 };
  struct userrec *u = NULL;

  u = get_user_by_handle(userlist, par);

  if (!par[0])
    dprintf(idx, "Usage: su <user>\n");
  else if (!u || (u && !whois_access(dcc[idx].user, u)))
    dprintf(idx, "No such user.\n");
  else if (u->bot)
    dprintf(idx, "You can't su to a bot... then again, why would you wanna?\n");
  else if (dcc[idx].u.chat->su_nick)
    dprintf(idx, "You cannot currently double .su; try .su'ing directly.\n");
  else {
    get_user_flagrec(u, &fr, NULL);
    ok = 1;

    if (conf.bot->hub) {
      if (!glob_huba(fr))
        ok = 0;
    } else {
      if (ischanhub() && !glob_chuba(fr))
        ok = 0;
    }
    if (!ok)
      dprintf(idx, "No party line access permitted for %s.\n", par);
    else {
      correct_handle(par);
      putlog(LOG_CMDS, "*", "#%s# su %s", dcc[idx].nick, par);
      if (!(atr & USER_OWNER) ||
	  ((u->flags & USER_OWNER) && (isowner(par)) &&
	   !(isowner(dcc[idx].nick)))) {
	/* This check is only important for non-owners */
	if (u_pass_match(u, "-")) {
	  dprintf(idx, "No password set for user. You may not .su to them.\n");
	  return;
	}
	if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
	  botnet_send_part_idx(idx, "");
	chanout_but(-1, dcc[idx].u.chat->channel,
		    "*** %s left the party line.\n", dcc[idx].nick);
	/* Store the old nick in the away section, for weenies who can't get
	 * their password right ;)
	 */
	if (dcc[idx].u.chat->away != NULL)
	  free(dcc[idx].u.chat->away);
        dcc[idx].u.chat->away = strdup(dcc[idx].nick);
        dcc[idx].u.chat->su_nick = strdup(dcc[idx].nick);
        dcc[idx].u.chat->su_channel = dcc[idx].u.chat->channel;

	dcc[idx].user = u;
	strlcpy(dcc[idx].nick, par, sizeof(dcc[idx].nick));
	/* Display password prompt and turn off echo (send IAC WILL ECHO). */
	dprintf(idx, "Enter password for %s%s\n", par,
		(dcc[idx].status & STAT_TELNET) ? TLN_IAC_C TLN_WILL_C
	       					  TLN_ECHO_C : "");
	dcc[idx].type = &DCC_CHAT_PASS;
      } else if (atr & USER_ADMIN) {
	if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
	  botnet_send_part_idx(idx, "");
	chanout_but(-1, dcc[idx].u.chat->channel,
		    "*** %s left the party line.\n", dcc[idx].nick);
	dprintf(idx, "Setting your username to %s.\n", par);
	if (atr & USER_MASTER)
	  dcc[idx].u.chat->con_flags = conmask;
        dcc[idx].u.chat->su_nick = strdup(dcc[idx].nick);
        dcc[idx].u.chat->su_channel = dcc[idx].u.chat->channel;
	dcc[idx].user = u;
	strlcpy(dcc[idx].nick, par, sizeof(dcc[idx].nick));
	dcc_chatter(idx);
      }
    }
  }
}

static void cmd_fixcodes(int idx, char *par)
{
  if (dcc[idx].status & STAT_TELNET) {
    dcc[idx].status |= STAT_ECHO;
    dcc[idx].status &= ~STAT_TELNET;
    dprintf(idx, "Turned off telnet codes.\n");
    putlog(LOG_CMDS, "*", "#%s# fixcodes (telnet off)", dcc[idx].nick);
  } else {
    dcc[idx].status |= STAT_TELNET;
    dcc[idx].status &= ~STAT_ECHO;
    dprintf(idx, "Turned on telnet codes.\n");
    putlog(LOG_CMDS, "*", "#%s# fixcodes (telnet on)", dcc[idx].nick);
  }
}

static void cmd_page(int idx, char *par)
{
  int a;

  if (!par[0]) {
    if (dcc[idx].status & STAT_PAGE) {
      dprintf(idx, "Currently paging outputs to %d lines.\n",
	      dcc[idx].u.chat->max_line);
    } else
      dprintf(idx, "You don't have paging on.\n");
    return;
  }
  a = atoi(par);
  if ((!a && !par[0]) || !strcasecmp(par, "off")) {
    dcc[idx].status &= ~STAT_PAGE;
    dcc[idx].u.chat->max_line = 0x7ffffff;	/* flush_lines needs this */
    while (dcc[idx].u.chat->buffer)
      flush_lines(idx, dcc[idx].u.chat);
    dprintf(idx, "Paging turned off.\n");
    putlog(LOG_CMDS, "*", "#%s# page off", dcc[idx].nick);
  } else if (a > 0) {
    dprintf(idx, "Paging turned on, stopping every %d line%s.\n", a,
        (a != 1) ? "s" : "");
    dcc[idx].status |= STAT_PAGE;
    dcc[idx].u.chat->max_line = a;
    dcc[idx].u.chat->line_count = 0;
    dcc[idx].u.chat->current_lines = 0;
    putlog(LOG_CMDS, "*", "#%s# page %d", dcc[idx].nick, a);
  } else {
    dprintf(idx, "Usage: page <off or #>\n");
    return;
  }
  console_dostore(idx);
}

static void cmd_newleaf(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: newleaf <handle> [host] [anotherhost]\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# newleaf %s", dcc[idx].nick, par);

  char *handle = newsplit(&par);

  if (strlen(handle) > HANDLEN)
    handle[HANDLEN] = 0;
  if (get_user_by_handle(userlist, handle))
    dprintf(idx, "Already got a %s user/bot\n", handle);
  else if (strchr(BADHANDCHARS, handle[0]) != NULL)
    dprintf(idx, "You can't start a botnick with '%c'.\n", handle[0]);
  else {
    struct userrec *u1 = NULL;
    struct bot_addr *bi = NULL;
    char tmp[81] = "", *host = NULL, *p = NULL, *hostname = NULL, *ip = NULL, *ip6 = NULL, *bhostname = NULL;
    int af_type = 0;

    userlist = adduser(userlist, handle, "none", "-", USER_OP, 1);
    u1 = get_user_by_handle(userlist, handle);
    bi = (struct bot_addr *) calloc(1, sizeof(struct bot_addr));
    bi->uplink = (char *) calloc(1, 1);
    bi->address = (char *) calloc(1, 1);
    bi->telnet_port = 3333;
    bi->relay_port = 3333;
    bi->hublevel = 999;
    set_user(&USERENTRY_BOTADDR, u1, bi);
    /* set_user(&USERENTRY_PASS, u1, settings.salt2); */
    simple_snprintf(tmp, sizeof(tmp), "%li %s", (long) now, dcc[idx].nick);
    set_user(&USERENTRY_ADDED, u1, tmp);
    dprintf(idx, "Added new leaf: %s\n", handle);
    while (par[0]) {
      host = newsplit(&par);
      addhost_by_handle(handle, host);
      dprintf(idx, "Added host '%s' to leaf: %s\n", host, handle);
      if ((p = strchr(host, '@'))) {
        hostname = ++p;
        af_type = is_dotted_ip(hostname);
        if (!ip && af_type == AF_INET)
          ip = strdup(hostname);
#ifdef USE_IPV6
        else if (!ip6 && af_type == AF_INET6)
          ip6 = strdup(hostname);
#endif /* USE_IPV6 */
        else if (!bhostname && !strchr(hostname, '*') && !strchr(hostname, '?'))
          bhostname = strdup(hostname);
      }
    }
    dprintf(idx, "Bot config line (prefix host with '+' if ipv6):\n");
    dprintf(idx, "%s %s %s %s\n", handle, ip ? ip : "*", bhostname ? bhostname : "*", ip6 ? ip6 : "");
    if (ip) free(ip);
    if (ip6) free(ip6);
    if (bhostname) free(bhostname);
    write_userfile(idx);
  }
}

static void cmd_newhub(int idx, char *par)
{
  bd::Array<bd::String> params(bd::String(par).split(' '));
  if (!par[0] || params.length() < 3) {
    dprintf(idx, "Usage: newhub <handle> <address> <port> [hublevel]\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# newhub %s", dcc[idx].nick, par);

  bd::String handle(params[0]);

  if (handle.length() > HANDLEN) {
    handle.resize(HANDLEN);
  }
  if (get_user_by_handle(userlist, const_cast<char*>(handle.c_str()))) {
    dprintf(idx, "Already got a %s hub\n", handle.c_str());
    return;
  } else if (strchr(BADHANDCHARS, handle[0]) != NULL) {
    dprintf(idx, "You can't start a botnick with '%c'.\n", static_cast<char>(handle[0]));
    return;
  }

  bd::String address(params[1]);
  in_port_t port = atoi(static_cast<bd::String>(params[2]).c_str());

  unsigned short hublevel = 0;
  // Was a hublevel passed in?
  if (params.length() == 4) {
    hublevel = atoi(static_cast<bd::String>(params[3]).c_str());
  } else {
    // Find next available hublevel
    for (struct userrec *u = userlist; u; u = u->next) {
      unsigned short u_bot_hublevel = bot_hublevel(u);
      if (u->bot && u_bot_hublevel < 999 && u_bot_hublevel > hublevel) {
        hublevel = u_bot_hublevel;
      }
    }
    ++hublevel;
  }

  struct bot_addr *bi = NULL;
  struct userrec *u1 = NULL;
  char tmp[81] = "";

  userlist = adduser(userlist, handle.c_str(), "none", "-", USER_OP, 1);
  u1 = get_user_by_handle(userlist, const_cast<char*>(handle.c_str()));
  bi = (struct bot_addr *) calloc(1, sizeof(struct bot_addr));
  bi->uplink = (char *) calloc(1, 1);
  bi->address = strdup(address.c_str());
  bi->telnet_port = port;
  bi->relay_port = port;
  bi->hublevel = hublevel;

  set_user(&USERENTRY_BOTADDR, u1, bi);
  simple_snprintf(tmp, sizeof(tmp), "%li %s", (long) now, dcc[idx].nick);
  set_user(&USERENTRY_ADDED, u1, tmp);
  dprintf(idx, "Added new hub: %s\n", handle.c_str());

  write_userfile(idx);
}

static void cmd_nopass(int idx, char *par)
{
  int cnt = 0;
  struct userrec *cu = NULL;
  char *users = (char *) calloc(1, 1), pass[MAXPASSLEN + 1] = "";
  size_t userssiz = 1;
  bool dopass = 0;

  putlog(LOG_CMDS, "*", "#%s# nopass %s", dcc[idx].nick, par[0] ? par : "");

  if (par[0])
    dopass = 1;

  for (cu = userlist; cu; cu = cu->next) {
    if (!cu->bot) {
      if (whois_access(dcc[idx].user, cu) && u_pass_match(cu, "-")) {
        cnt++;
        if (dopass) {
          pass[0] = 0;
          make_rand_str(pass, MAXPASSLEN);
          set_user(&USERENTRY_PASS, cu, pass);
        } else {
          userssiz = strlen(users) + strlen(cu->handle) + 1 + 1;
          users = (char *) realloc(users, userssiz);
          strlcat(users, cu->handle, userssiz);
          strlcat(users, " ", userssiz);
        }
      }
    }
  }
  if (!cnt) 
    dprintf(idx, "All users have passwords set.\n");
  else if (dopass) {
    dprintf(idx, "%d users without passwords set to random.\n", cnt);
    write_userfile(idx);
  } else
    dprintf(idx, "Users without passwords: %s\n", users);
  free(users);
}

static void cmd_pls_ignore(int idx, char *par)
{
  char *who = NULL, s[UHOSTLEN] = "";
  unsigned long int expire_time = 0;

  if (!par[0]) {
    dprintf(idx, "Usage: +ignore <hostmask> [%%<XdXhXm>] [comment]\n");
    return;
  }

  who = newsplit(&par);
  if (par[0] == '%') {
    char *p = NULL, *p_expire = NULL;
    unsigned long int expire_foo;

    p = newsplit(&par);
    p_expire = p + 1;
    while (*(++p) != 0) {
      switch (tolower(*p)) {
      case 'd':
	*p = 0;
	expire_foo = strtol(p_expire, NULL, 10);
	if (expire_foo > 365)
	  expire_foo = 365;
	expire_time += 86400 * expire_foo;
	p_expire = p + 1;
	break;
      case 'h':
	*p = 0;
	expire_foo = strtol(p_expire, NULL, 10);
	if (expire_foo > 8760)
	  expire_foo = 8760;
	expire_time += 3600 * expire_foo;
	p_expire = p + 1;
	break;
      case 'm':
	*p = 0;
	expire_foo = strtol(p_expire, NULL, 10);
	if (expire_foo > 525600)
	  expire_foo = 525600;
	expire_time += 60 * expire_foo;
	p_expire = p + 1;
      }
    }
  }
  if (!par[0])
    par = "requested";
  else if (strlen(par) > 65)
    par[65] = 0;
  if (strlen(who) > UHOSTMAX - 4)
    who[UHOSTMAX - 4] = 0;

  /* Fix missing ! or @ BEFORE continuing */
  if (!strchr(who, '!')) {
    if (!strchr(who, '@'))
      simple_snprintf(s, sizeof(s), "%s!*@*", who);
    else
      simple_snprintf(s, sizeof(s), "*!%s", who);
  } else if (!strchr(who, '@'))
    simple_snprintf(s, sizeof(s), "%s@*", who);
  else
    strlcpy(s, who, sizeof(s));

  if (match_ignore(s))
    dprintf(idx, "That already matches an existing ignore.\n");
  else {
    dprintf(idx, "Now ignoring: %s (%s)\n", s, par);
    addignore(s, dcc[idx].nick, (const char *) par, expire_time ? now + expire_time : 0L);
    putlog(LOG_CMDS, "*", "#%s# +ignore %s %s", dcc[idx].nick, s, par);
    if (conf.bot->hub)
      write_userfile(idx);
  }
}

static void cmd_mns_ignore(int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, "Usage: -ignore <hostmask | ignore #>\n");
    return;
  }

  char buf[UHOSTLEN] = "", *expanded_buf = NULL;

  strlcpy(buf, par, sizeof buf);
  if ((expanded_buf = delignore(buf)) && expanded_buf[0]) {
    putlog(LOG_CMDS, "*", "#%s# -ignore %s", dcc[idx].nick, buf);
    dprintf(idx, "No longer ignoring: %s\n", expanded_buf);
    if (conf.bot->hub)
      write_userfile(idx);
  } else
    dprintf(idx, "That ignore cannot be found.\n");
}

static void cmd_ignores(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# ignores %s", dcc[idx].nick, par);
  tell_ignores(idx, par);
}

static void cmd_pls_user(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# +user %s", dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, "Usage: +user <handle> [host] [anotherhost] ...\n");
    return;
  }

  char *handle = newsplit(&par), *host = newsplit(&par);

  if (strlen(handle) > HANDLEN)
    handle[HANDLEN] = 0;
  if (get_user_by_handle(userlist, handle))
    dprintf(idx, "Someone already exists by that name.\n");
  else if (strchr(BADHANDCHARS, handle[0]) != NULL)
    dprintf(idx, "You can't start a nick with '%c'.\n", handle[0]);
  else if (!strcasecmp(handle, conf.bot->nick))
    dprintf(idx, "Hey! That's MY name!\n");
  else {
    struct userrec *u2 = NULL;
    char tmp[50] = "", s[MAXPASSLEN + 1] = "", s2[MAXPASSLEN + 1] = "";

    char ahost[UHOSTLEN] = "", *phost = NULL;

    if (host[0] && !strchr(host, '!')) {
      if (!strchr(host, '@')) {
        simple_snprintf(ahost, sizeof(ahost), "*!*@%s", host);
      } else
        simple_snprintf(ahost, sizeof(ahost), "*!%s", host);

      phost = ahost;
    } else
      phost = host;


    userlist = adduser(userlist, handle, phost, "-", USER_DEFAULT, 0);
    u2 = get_user_by_handle(userlist, handle);
    simple_snprintf(tmp, sizeof(tmp), "%li %s", (long)now, dcc[idx].nick);
    set_user(&USERENTRY_ADDED, u2, tmp);
    dprintf(idx, "Added %s (%s) with no flags.\n", handle, phost[0] ? phost : "no host");
    while (par[0]) {
      phost = 0;
      ahost[0] = 0;
      host = newsplit(&par);

      if (!strchr(host, '!')) {
        if (!strchr(host, '@')) {
          simple_snprintf(ahost, sizeof(ahost), "*!*@%s", host);
        } else
          simple_snprintf(ahost, sizeof(ahost), "*!%s", host);

        phost = ahost;
      } else
        phost = host;

      addhost_by_handle(handle, phost);
      dprintf(idx, "Added host '%s' to %s.\n", phost, handle);
    }
    make_rand_str(s, MAXPASSLEN);
    set_user(&USERENTRY_PASS, u2, s);

    make_rand_str(s2, MAXPASSLEN);
    set_user(&USERENTRY_SECPASS, u2, s2);
    dprintf(idx, "%s's initial password set to %s%s%s\n", handle, BOLD(idx), s, BOLD_END(idx));
    dprintf(idx, "%s's initial secpass set to %s%s%s\n", handle, BOLD(idx), s2, BOLD_END(idx));

    if (conf.bot->hub)
      write_userfile(idx);
  }
}

static void cmd_mns_user(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# -user %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: -user <handle> [anotherhandle] ...\n");
    return;
  }

  struct userrec *u2 = NULL;

  while (par[0]) {
    char *handle = newsplit(&par);
    u2 = get_user_by_handle(userlist, handle);

    if (!u2 || (u2 && !whois_access(dcc[idx].user, u2))) {
      dprintf(idx, "No such user: %s\n", handle);
      continue;
    }
    if (isowner(handle)) {
      dprintf(idx, "A hard-coded owner is unremoveable: %s\n", handle);
      continue;
    }
    if ((u2->flags & USER_OWNER) && !(dcc[idx].user->flags & USER_OWNER)) {
      dprintf(idx, "You can't remove a bot owner: %s\n", handle);
      continue;
    }

    if (u2->bot) {
      if (!(dcc[idx].user->flags & USER_OWNER)) {
          dprintf(idx, "You can't remove bots!");
          continue;
      }
      if (!strcasecmp(u2->handle, conf.bot->nick)) {
        dprintf(idx, "Use 'suicide' to kill me.\n");
        continue;
      }

      int i = nextbot(handle);

      if (i < 0)
        botunlink(idx, handle, "Bot removed.");
      else if (!strcasecmp(dcc[i].nick, handle))
        botunlink(idx, handle, "Bot removed.");
      else {
        char x[40] = "";

        simple_snprintf(x, sizeof(x), "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, conf.bot->nick);
        botnet_send_unlink(i, x, lastbot(handle), handle, "Bot removed.");
      }
    }

    if (!conf.bot->hub)
      check_this_user(handle, 1, NULL);
    if (deluser(handle)) {
      dprintf(idx, "Removed %s: %s.\n", u2->bot ? "bot" : "user", handle);
    } else
      dprintf(idx, "Failed to remove %s: %s.\n", u2->bot ? "bot" : "user", handle);
  }

  if (conf.bot->hub)
    write_userfile(idx);
}

static void cmd_pls_host(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# +host %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: +host [handle] <newhostmask> [anotherhost] ...\n");
    return;
  }

  char *handle = newsplit(&par), *host = NULL;
  struct userrec *u2 = NULL;

  if (par[0]) {
    host = newsplit(&par);
    u2 = get_user_by_handle(userlist, handle);
  } else {
    host = handle;
    handle = dcc[idx].nick;
    u2 = dcc[idx].user;
  }
  if (!u2 || (u2 && !whois_access(dcc[idx].user, u2))) {
    dprintf(idx, "No such user.\n");
    return;
  }

  struct flag_record fr2 = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 },
                     fr  = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &fr, NULL);
  if (strcasecmp(handle, dcc[idx].nick)) {
    get_user_flagrec(u2, &fr2, NULL);
    if (!glob_master(fr) && !glob_bot(fr2) && !chan_master(fr)) {
      dprintf(idx, "You can't add hostmasks to non-bots.\n");
      return;
    }
    if (!glob_owner(fr) && glob_bot(fr2)) {
      dprintf(idx, "You can't add hostmasks to bots.\n");
      return;
    }
    if (glob_admin(fr2) && !isowner(dcc[idx].nick)) {
      dprintf(idx, "You can't add hostmasks to an admin.\n");
      return;
    }
    if ((glob_owner(fr2) || glob_master(fr2)) && !glob_owner(fr)) {
      dprintf(idx, "You can't add hostmasks to a bot owner/master.\n");
      return;
    }
    if ((chan_owner(fr2) || chan_master(fr2)) && !glob_master(fr) &&
        !glob_owner(fr) && !chan_owner(fr)) {
      dprintf(idx, "You can't add hostmasks to a channel owner/master.\n");
      return;
    }
    if (!glob_master(fr) && !chan_master(fr)) {
      dprintf(idx, "Permission denied.\n");
      return;
    }
  }
  if (!chan_master(fr) && get_user_by_host(host)) {
    dprintf(idx, "You cannot add a host matching another user!\n");
    return;
  }
  
  char ahost[UHOSTLEN] = "", *phost = NULL;

  if (!strchr(host, '!')) {
    if (!strchr(host, '@')) {
      simple_snprintf(ahost, sizeof(ahost), "*!*@%s", host);
    } else
      simple_snprintf(ahost, sizeof(ahost), "*!%s", host);

    phost = ahost;
  } else
    phost = host;

  if (user_has_host(NULL, dcc[idx].user, phost))
    dprintf(idx, "The hostmask '%s' is already there.\n", phost);
  else {
    addhost_by_handle(handle, phost);
    update_mod(handle, dcc[idx].nick, "+host", phost);
    dprintf(idx, "Added host '%s' to %s.\n", phost, handle);
  }

  while (par[0]) {
    phost = 0;
    ahost[0] = 0;
    host = newsplit(&par);

    if (!strchr(host, '!')) {
      if (!strchr(host, '@')) {
        simple_snprintf(ahost, sizeof(ahost), "*!*@%s", host);
      } else
        simple_snprintf(ahost, sizeof(ahost), "*!%s", host);

      phost = ahost;
    } else
      phost = host;

    if (user_has_host(NULL, dcc[idx].user, phost))
      dprintf(idx, "The hostmask '%s' is already there.\n", phost);
    else {
      addhost_by_handle(handle, phost);
      dprintf(idx, "Added host '%s' to %s.\n", phost, handle);
    }
  }
  if (!conf.bot->hub)
    check_this_user(handle, 0, NULL);
  else
    write_userfile(idx);
}

static void cmd_mns_host(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# -host %s", dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, "Usage: -host [handle] <hostmask> [anotherhost] ...\n");
    return;
  }

  char *handle = newsplit(&par), *host = NULL;
  struct userrec *u2 = NULL;

  if (par[0]) {
    host = newsplit(&par);
    u2 = get_user_by_handle(userlist, handle);
  } else {
    host = handle;
    handle = dcc[idx].nick;
    u2 = dcc[idx].user;
  }
  if (!u2 || (u2 && !whois_access(dcc[idx].user, u2))) {
    dprintf(idx, "No such user.\n");
    return;
  }

  struct flag_record fr2 = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 },
                     fr  = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &fr, NULL);
  get_user_flagrec(u2, &fr2, NULL);

  /* check to see if user is +d or +k and don't let them remove hosts from THEMSELVES*/
  if (dcc[idx].user == u2 && (glob_deop(fr) || glob_kick(fr) || (!glob_master(fr) && (chan_deop(fr) || chan_kick (fr))))) {
    dprintf(idx, "You can't remove hostmasks from yourself while having the +d or +k flag.\n");
      return;
    }

  if (strcasecmp(handle, dcc[idx].nick)) {
    if (!glob_master(fr) && !glob_bot(fr2) && !chan_master(fr)) {
      dprintf(idx, "You can't remove hostmasks from non-bots.\n");
      return;
    }
    if (glob_bot(fr2) && !glob_owner(fr)) {
      dprintf(idx, "You can't remove hostmasks from bots.\n");
      return;
    }
    if (glob_admin(fr2) && !isowner(dcc[idx].nick)) {
      dprintf(idx, "You can't remove hostmasks from an admin.\n");
      return;
    }
    if ((glob_owner(fr2) || glob_master(fr2)) && !glob_owner(fr)) {
      dprintf(idx, "You can't remove hostmasks from a bot owner/master.\n");
      return;
    }
    if ((chan_owner(fr2) || chan_master(fr2)) && !glob_master(fr) &&
        !glob_owner(fr) && !chan_owner(fr)) {
      dprintf(idx, "You can't remove hostmasks from a channel owner/master.\n");
      return;
    }
    if (!glob_master(fr) && !chan_master(fr)) {
      dprintf(idx, "Permission denied.\n");
      return;
    }
  }

  char ahost[UHOSTLEN] = "", *phost = NULL;

  if (!strchr(host, '!')) {
    if (!strchr(host, '@')) {
      simple_snprintf(ahost, sizeof(ahost), "*!*@%s", host);
    } else
      simple_snprintf(ahost, sizeof(ahost), "*!%s", host);

    phost = ahost;
  } else
    phost = host;

  if (delhost_by_handle(handle, phost)) {
    dprintf(idx, "Removed host '%s' from %s.\n", phost, handle);
    update_mod(handle, dcc[idx].nick, "-host", phost);

    while (par[0]) {
      phost = 0;
      ahost[0] = 0;
      host = newsplit(&par);

      if (!strchr(host, '!')) {
        if (!strchr(host, '@')) {
          simple_snprintf(ahost, sizeof(ahost), "*!*@%s", host);
        } else
          simple_snprintf(ahost, sizeof(ahost), "*!%s", host);

        phost = ahost;
      } else
        phost = host;

      if (delhost_by_handle(handle, phost)) {
        if (!conf.bot->hub)
          check_this_user(handle, 2, phost);
        dprintf(idx, "Removed host '%s' from %s.\n", phost, handle);
      }
    }

    if (conf.bot->hub)
      write_userfile(idx);
  } else
    dprintf(idx, "Failed.\n");
}

static void cmd_clearhosts(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# clearhosts %s", dcc[idx].nick, par);

  char *handle = NULL;
  struct userrec *u2 = NULL;

  if (par[0]) {
    handle = newsplit(&par);
    u2 = get_user_by_handle(userlist, handle);
  } else {
    handle = dcc[idx].nick;
    u2 = dcc[idx].user;
  }

  if (!u2 || (u2 && !whois_access(dcc[idx].user, u2))) {
    dprintf(idx, "No such user.\n");
    return;
  }

  struct flag_record fr2 = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 },
                     fr  = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &fr, NULL);
  get_user_flagrec(u2, &fr2, NULL);

  /* check to see if user is +d or +k and don't let them remove hosts from THEMSELVES*/
  if (dcc[idx].user == u2 && (glob_deop(fr) || glob_kick(fr) || (!glob_master(fr) && (chan_deop(fr) || chan_kick (fr))))) {
    dprintf(idx, "You can't remove hostmasks from yourself while having the +d or +k flag.\n");
      return;
    }

  if (strcasecmp(handle, dcc[idx].nick)) {
    if (!glob_master(fr) && !glob_bot(fr2) && !chan_master(fr)) {
      dprintf(idx, "You can't remove hostmasks from non-bots.\n");
      return;
    }
    if (glob_bot(fr2) && !glob_owner(fr)) {
      dprintf(idx, "You can't remove hostmasks from bots.\n");
      return;
    }
    if (glob_admin(fr2) && !isowner(dcc[idx].nick)) {
      dprintf(idx, "You can't remove hostmasks from an admin.\n");
      return;
    }
    if ((glob_owner(fr2) || glob_master(fr2)) && !glob_owner(fr)) {
      dprintf(idx, "You can't remove hostmasks from a bot owner/master.\n");
      return;
    }
    if ((chan_owner(fr2) || chan_master(fr2)) && !glob_master(fr) &&
        !glob_owner(fr) && !chan_owner(fr)) {
      dprintf(idx, "You can't remove hostmasks from a channel owner/master.\n");
      return;
    }
    if (!glob_master(fr) && !chan_master(fr)) {
      dprintf(idx, "Permission denied.\n");
      return;
    }
  }

  if (get_user(&USERENTRY_HOSTS, u2)) {
    shareout("ch %s\n", handle);
    noshare = 1;
    set_user(&USERENTRY_HOSTS, u2, (void *) "none");
    noshare = 0;
    dprintf(idx, "Cleared hosts for %s.\n", handle);
    if (!conf.bot->hub && server_online)
      check_this_user(handle, 1, NULL);
  } else
    dprintf(idx, "%s had no hosts set.\n", handle);

  if (conf.bot->hub)
    write_userfile(idx);
}

/* netserver */
static void cmd_netserver(int idx, char * par) {
  putlog(LOG_CMDS, "*", "#%s# netserver", dcc[idx].nick);
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, "cursrv");
}

static void cmd_botserver(int idx, char * par) {
  putlog(LOG_CMDS, "*", "#%s# botserver %s", dcc[idx].nick, par);
  if (!par || !par[0]) {
    dprintf(idx, "Usage: botserver <bot>\n");
    return;
  }
  if (strcasecmp(conf.bot->nick, par) && nextbot(par)<0) {
    dprintf(idx, "%s isn't a linked bot.\n", par);
    return;
  }
  botnet_send_cmd(conf.bot->nick, par, dcc[idx].nick, idx, "cursrv");
}


static void rcmd_cursrv(char * fbot, char * fhand, char * fidx) {
  if (!conf.bot->hub) {
    char cursrv[120] = "", tmp[30] = "";

    if (server_online) {
      daysdur(now, server_online, tmp, sizeof(tmp));
      simple_snprintf(cursrv, sizeof(cursrv), "Currently: %-30s (connected %s)%s%s", cursrvname, tmp, socklist[findanysnum(serv)].ssl ? " [SSL]" : "", floodless ? " [floodless]" : "");

      if (server_lag > 0) {
        simple_snprintf(tmp, sizeof(tmp), " Lag:%ds", server_lag);
        strlcat(cursrv, tmp, sizeof(cursrv));
      }
    } else
      strlcpy(tmp, "Currently: none", sizeof(tmp));

    botnet_send_cmdreply(conf.bot->nick, fbot, fhand, fidx, cursrv);
  }
}

static void cmd_timesync(int idx, char *par) {
  char tmp[30] = "";

  putlog(LOG_CMDS, "*", "#%s# timesync", dcc[idx].nick);
  simple_snprintf(tmp, sizeof(tmp), "timesync %li", (long)(timesync + now));
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, tmp);
}

static void rcmd_timesync(char *frombot, char *fromhand, char *fromidx, char *par) {
  char tmp[100] = "";
  long net = atol(par);

  simple_snprintf(tmp, sizeof(tmp), "NET: %li    ME: %li   DIFF: %d", net, (long)(timesync + now), (int) ((timesync+now) - net));
  botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, tmp);
}

/* netversion */
static void cmd_netversion(int idx, char * par) {
  putlog(LOG_CMDS, "*", "#%s# netversion", dcc[idx].nick);
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, "ver");
}

static void cmd_botversion(int idx, char * par) {
  putlog(LOG_CMDS, "*", "#%s# botversion %s", dcc[idx].nick, par);
  if (!par || !par[0]) {
    dprintf(idx, "Usage: botversion <bot>\n");
    return;
  }
  if (strcasecmp(conf.bot->nick, par) && nextbot(par)<0) {
    dprintf(idx, "%s isn't a linked bot.\n", par);
    return;
  }
  botnet_send_cmd(conf.bot->nick, par, dcc[idx].nick, idx, "ver");
}

static void rcmd_ver(char * fbot, char * fhand, char * fidx) {
  char tmp[401] = "";
  struct utsname un;

  simple_snprintf(tmp, sizeof(tmp), "%s ", version);
  if (uname(&un) < 0) {
    strlcat(tmp, "(unknown OS)", sizeof(tmp));
  } else {
    if (updated) {
      simple_snprintf(&tmp[strlen(tmp)], sizeof(tmp) - strlen(tmp), " %s %s (%s) - UPDATED", un.sysname, un.release, un.machine);
    } else
      simple_snprintf(&tmp[strlen(tmp)], sizeof(tmp) - strlen(tmp), " %s %s (%s)", un.sysname, un.release, un.machine);
  }
  botnet_send_cmdreply(conf.bot->nick, fbot, fhand, fidx, tmp);
}

static void cmd_version(int idx, char *par)
{ 
  putlog(LOG_CMDS, "*", "#%s# version", dcc[idx].nick);
  botnet_send_cmd(conf.bot->nick, conf.bot->nick, dcc[idx].nick, idx, "ver");
}

/* netnick, botnick, nick */
static void cmd_netnick (int idx, char *par) {
  putlog(LOG_CMDS, "*", "#%s# netnick", dcc[idx].nick);
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, "curnick");
}

static void cmd_netrelease (int idx, char* par) {
  putlog(LOG_CMDS, "*", "#%s# netrelease %s", dcc[idx].nick, par);
  if (!par || !par[0]) {
    dprintf(idx, "Usage: netrelease <nick>\n");
    return;
  }

  // Don't bother checking what bot has the nick, multiple bots may be trying to jupe it
  // so instead, tell all bots to release the specified nick.
  // Any bots trying to get it will stop for 7 seconds, and any bot on it will release it.
  bd::String str;
  str = bd::String::printf("rn %s", par);
  putallbots(str.c_str());
}

static void cmd_botnick(int idx, char * par) {
  putlog(LOG_CMDS, "*", "#%s# botnick %s", dcc[idx].nick, par);
  if (!par || !par[0]) {
    dprintf(idx, "Usage: botnick <bot>\n");
    return;
  }
  if (nextbot(par) < 0) {
    dprintf(idx, "%s isn't a linked bot\n", par);
  }
  botnet_send_cmd(conf.bot->nick, par, dcc[idx].nick, idx, "curnick");
}

static void rcmd_curnick(char * fbot, char * fhand, char * fidx) {
  if (!conf.bot->hub) {
    char tmp[301] = "";

    simple_snprintf(tmp, sizeof(tmp), "Currently: %-20s ",
        server_online ? botname : "(not online)");
    if (jupenick[0] && strncmp(botname, jupenick, strlen(botname)))
      simple_snprintf(&tmp[strlen(tmp)], sizeof(tmp) - strlen(tmp), "Jupe: %s ", jupenick);
    else if (jupenick[0] && strcmp(botname, origbotname))
      simple_snprintf(&tmp[strlen(tmp)], sizeof(tmp) - strlen(tmp), "Jupe: %s Main: %s ", jupenick, origbotname);
    else if (strcmp(botname, origbotname))
      simple_snprintf(&tmp[strlen(tmp)], sizeof(tmp) - strlen(tmp), "Main: %s ", origbotname);
    botnet_send_cmdreply(conf.bot->nick, fbot, fhand, fidx, tmp);
  }
}

/* botmsg */
static void cmd_botmsg(int idx, char * par) {
  char *tnick = NULL, *tbot = NULL;

  putlog(LOG_CMDS, "*", "#%s# botmsg %s", dcc[idx].nick, par);
  tbot = newsplit(&par);
  if (par[0])
    tnick = newsplit(&par);
  if (!par[0]) {
    dprintf(idx, "Usage: botmsg <bot> <nick|#channel> <message>\n");
    return;
  }
  if (strcasecmp(conf.bot->nick, tbot) && nextbot(tbot)<0) {
    dprintf(idx, "%s isn't a linked bot.\n", tbot);
    return;
  }
  
  char tmp[1024] = "";

  simple_snprintf(tmp, sizeof tmp, "msg %s %s", tnick, par);
  botnet_send_cmd(conf.bot->nick, tbot, dcc[idx].nick, idx, tmp);
}

static void rcmd_msg(char * tobot, char * frombot, char * fromhand, char * fromidx, char * par) {
  if (!conf.bot->hub) {
    char *nick = newsplit(&par);

    privmsg(nick, par, DP_SERVER);
    if (!strcasecmp(tobot, conf.bot->nick)) {
      char buf[1024] = "";

      simple_snprintf(buf, sizeof buf, "Sent message to %s", nick);
      botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, buf);
    }
  }
}

/* netlag */
static void cmd_netlag(int idx, char * par) {
  egg_timeval_t tv;
  char tmp[64] = "";

  putlog(LOG_CMDS, "*", "#%s# netlag", dcc[idx].nick);
  
  timer_get_now(&tv);
  simple_snprintf(tmp, sizeof(tmp), "ping %li", (long) ((tv.sec % 10000) * 100 + (tv.usec * 100) / (1000000)));
  dprintf(idx, "Sent ping to all linked bots\n");
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, tmp);
}

static void rcmd_ping(char * frombot, char *fromhand, char * fromidx, char * par) {
  char tmp[64] = "";

  simple_snprintf(tmp, sizeof tmp, "pong %s", par);
  botnet_send_cmd(conf.bot->nick, frombot, fromhand, atoi(fromidx), tmp);
}

static void rcmd_pong(char *frombot, char *fromhand, char *fromidx, char *par) {
  int i = atoi(fromidx);

  if ((i >= 0) && (i < dcc_total) && (dcc[i].type == &DCC_CHAT) && (!strcmp(dcc[i].nick, fromhand))) {
    egg_timeval_t tv;

    timer_get_now(&tv);
    long tm = ((tv.sec % 10000) * 100 + (tv.usec * 100) / (1000000)) - atol(par);
    dprintf(i, "Pong from %s: %d.%d seconds\n", frombot, (int)(tm / 100), (int)(tm % 100));
  }
}

/* exec commands */
static void cmd_netw(int idx, char * par) {

  putlog(LOG_CMDS, "*", "#%s# netw", dcc[idx].nick);

  char tmp[7] = "";
  strlcpy(tmp, "exec w", sizeof(tmp));
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, tmp);
}

static void cmd_netps(int idx, char * par) {
  putlog(LOG_CMDS, "*", "#%s# netps %s", dcc[idx].nick, par);

  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>') || strchr(par, '`') || strchr(par, '$')) {
    putlog(LOG_WARN, "*", "%s attempted 'netps' with pipe/semicolon in parameters: %s", dcc[idx].nick, par);
    dprintf(idx, "No.\n");
    return;
  }

  char buf[1024] = "";

  simple_snprintf(buf, sizeof(buf), "exec ps %s", par);
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, buf);
}

static void cmd_netlast(int idx, char * par) {
  putlog(LOG_CMDS, "*", "#%s# netlast %s", dcc[idx].nick, par);

  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>') || strchr(par, '`') || strchr(par, '$')) {
    putlog(LOG_WARN, "*", "%s attempted 'netlast' with pipe/semicolon in parameters: %s", dcc[idx].nick, par);
    dprintf(idx, "No.\n");
    return;
  }

  char buf[1024] = "";

  simple_snprintf(buf, sizeof(buf), "exec last %s", par);
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, buf);
}

void crontab_show(struct userrec *u, int idx) {
  dprintf(idx, "Showing current crontab:\n");
  bd::Stream crontab;
  bd::String line;
  crontab_exists(&crontab);
  crontab.seek(0, SEEK_SET);
  while (crontab.tell() < crontab.length()) {
    line = crontab.getline();
    dprintf(idx, "%s\n", line.c_str());
  }
}

static void cmd_crontab(int idx, char *par) {
  putlog(LOG_CMDS, "*", "#%s# crontab %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: crontab <status|delete|show|new> [interval]\n");
    return;
  }
  char *code = newsplit(&par);
  int i = 0;

  if (!strcmp(code, "status")) {
    i = crontab_exists();
    if (!i)
      dprintf(idx, "No crontab\n");
    else if (i==1)
      dprintf(idx, "Crontabbed\n");
    else
      dprintf(idx, "Error checking crontab status\n");
  } else if (!strcmp(code, "show")) {
    crontab_show(dcc[idx].user, idx);
  } else if (!strcmp(code, "delete")) {
    crontab_del();
    i = crontab_exists();
    if (!i)
      dprintf(idx, "No crontab\n");
    else if (i==1)
      dprintf(idx, "Crontabbed\n");
    else
      dprintf(idx, "Error checking crontab status\n");
  } else if (!strcmp(code, "new")) {
    i = atoi(par);
    if ((i <= 0) || (i > 60))
      i = 10;
    crontab_create(i);
    i = crontab_exists();
    if (!i)
      dprintf(idx, "No crontab\n");
    else if (i == 1)
      dprintf(idx, "Crontabbed\n");
    else
      dprintf(idx, "Error checking crontab status\n");
  } else {
    dprintf(idx, "Usage: crontab status|delete|show|new [interval]\n");
  }
}

static void my_dns_callback(int id, void *client_data, const char *host, bd::Array<bd::String> ips)
{
  //64bit hacks
  long data = (long) client_data;
  int idx = (int) data;

  Context;

  if (!valid_idx(idx))
    return;

  if (ips.size())
    for (size_t i = 0; i < ips.size(); ++i)
      dprintf(idx, "Resolved %s using (%s) to: %s\n", host, dns_ip, bd::String(ips[i]).c_str());
  else
    dprintf(idx, "Failed to lookup via (%s): %s\n", dns_ip, host);

  return;
}

static void cmd_dns(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# dns %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: dns <hostname/ip/flush>\n");
     return;
  }

  if (!strcasecmp(par, "flush")) {
    dprintf(idx, "Flushing cache...\n");
    dns_cache_flush();
    return;
  }
  if (is_dotted_ip(par)) {
    dprintf(idx, "Reversing %s ...\n", par);
    egg_dns_reverse(par, 20, my_dns_callback, (void *) (long) idx);

  } else {
    dprintf(idx, "Looking up %s ...\n", par);
    egg_dns_lookup(par, 20, my_dns_callback, (void *) (long) idx);
  }
}

static void cmd_netcrontab(int idx, char * par) 
{
  putlog(LOG_CMDS, "*", "#%s# netcrontab %s", dcc[idx].nick, par);

  char *cmd = newsplit(&par);

  if ((strcmp(cmd, "status") && strcmp(cmd, "show") && strcmp(cmd, "delete") && strcmp(cmd, "new"))) {
    dprintf(idx, "Usage: netcrontab <status|delete|show|new> [interval]\n");
    return;
  }

  char buf[1024] = "";

  simple_snprintf(buf, sizeof buf, "exec crontab %s %s", cmd, par);
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, buf);
}

static void rcmd_exec(char * frombot, char * fromhand, char * fromidx, char * par) {
  char *cmd = NULL, scmd[512] = "", *out = NULL, *err = NULL;

  cmd = newsplit(&par);
  if (!strcmp(cmd, "w")) {
    strlcpy(scmd, "w", 2);
  } else if (!strcmp(cmd, "last")) {
    char user[20] = "";

    if (par[0]) {
      strlcpy(user, par, sizeof(user));
    } else if (conf.username) {
      strlcpy(user, conf.username, sizeof(user));
    }
    if (!user[0]) {
      botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, "Can't determine user id for process");
      return;
    }
    simple_snprintf(scmd, sizeof scmd, "last %s", user);
  } else if (!strcmp(cmd, "ps")) {
    simple_snprintf(scmd, sizeof scmd, "ps %s", par);
  } else if (!strcmp(cmd, "crontab")) {
    char *code = newsplit(&par);

    if (!strcmp(code, "show")) {
      strlcpy(scmd, "crontab -l", sizeof(scmd));
    } else if (!strcmp(code, "delete")) {
      crontab_del();
    } else if (!strcmp(code, "new")) {
      int i=atoi(par);
      if ((i <= 0) || (i > 60))
        i = 10;
      crontab_create(i);
    }
    if (!scmd[0]) {
      char s[31] = "";
      int i = crontab_exists();

      if (!i)
        simple_snprintf(s, sizeof(s), "No crontab");
      else if (i == 1)
        simple_snprintf(s, sizeof(s), "Crontabbed");
      else
        simple_snprintf(s, sizeof(s), "Error checking crontab status");
      botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, s);
    }
  }
  if (!scmd[0])
    return;
  if (shell_exec(scmd, NULL, &out, &err, 1)) {
    if (out) {
      char *p = NULL, *np = NULL;

      botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, "Result:");
      p = out;
      while (p && p[0]) {
        np = strchr(p, '\n');
        if (np)
          *np++ = 0;
        botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, p);
        p = np;
      }
      free(out);
    }
    if (err) {
      char *p = NULL, *np = NULL;

      botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, "Errors:");
      p = err;
      while (p && p[0]) {
        np = strchr(p, '\n');
        if (np)
          *np++ = 0;
        botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, p);
        p = np;
      }
      free(err);
    }
  } else {
    botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, "exec failed");
  }

}

static void cmd_botjump(int idx, char * par) {
  char *tbot = NULL;

  putlog(LOG_CMDS, "*", "#%s# botjump %s", dcc[idx].nick, par);
  tbot = newsplit(&par);
  if (!tbot[0]) {
    dprintf(idx, "Usage: botjump <bot> [server [port [pass]]]\n");
    return;
  }
  if (strcasecmp(conf.bot->nick, tbot) && nextbot(tbot)<0) {
    dprintf(idx, "%s isn't a linked bot.\n", tbot);
    return;
  }

  char buf[1024] = "";

  simple_snprintf(buf, sizeof buf, "jump %s", par);
  botnet_send_cmd(conf.bot->nick, tbot, dcc[idx].nick, idx, buf);
}

static void rcmd_jump(char * frombot, char * fromhand, char * fromidx, char * par) {
  if (!conf.bot->hub) {
    if (par[0]) {
      char *other = newsplit(&par), *p = NULL;
      in_port_t port = atoi(newsplit(&par));

      if ((p = strchr(other, ':'))) {
        *p = 0;
        p++;
        if (!port)
          port = atoi(p);
      }

      if (!port)
        port = (ssl_use ? default_port_ssl : default_port);
      strlcpy(newserver, other, 120); 
      newserverport = port; 
      strlcpy(newserverpass, par, 120); 
    }
    botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, "Jumping...");

    nuke_server("Jumping...");
    cycle_time = 0;
  }
}

/* "Remotable" commands */
void gotremotecmd (char *forbot, char *frombot, char *fromhand, char *fromidx, char *cmd) 
{
  char *par = cmd;

  cmd = newsplit(&par);

  if (!strcmp(cmd, "exec")) {
    rcmd_exec(frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, "curnick")) {
    rcmd_curnick(frombot, fromhand, fromidx);
  } else if (!strcmp(cmd, "cursrv")) {
    rcmd_cursrv(frombot, fromhand, fromidx);
  } else if (!strcmp(cmd, "chans")) {
    rcmd_chans(frombot, fromhand, fromidx);
  } else if (!strcmp(cmd, "jump")) {
    rcmd_jump(frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, "msg")) {
    rcmd_msg(forbot, frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, "ver")) {
    rcmd_ver(frombot, fromhand, fromidx);
  } else if (!strcmp(cmd, "ping")) {
    rcmd_ping(frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, "pong")) {
    rcmd_pong(frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, "die")) {
    exit(0);
  } else if (!strcmp(cmd, "timesync")) {
    rcmd_timesync(frombot, fromhand, fromidx, par);
  } else {
    botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, "Unrecognized remote command");
  }
}
    
void gotremotereply (char *frombot, char *tohand, char *toidx, char *ln) {
  int idx = atoi(toidx);

  if ((idx >= 0) && (idx < dcc_total) && (dcc[idx].type == &DCC_CHAT) && (!strcmp(dcc[idx].nick, tohand))) {
    char *buf = NULL;
    size_t siz = strlen(frombot) + 2 + 1;

    buf = (char *) calloc(1, siz);

    simple_snprintf(buf, siz, "(%s)", frombot);
    char format[10] = "";
    simple_snprintf(format, sizeof(format), "%%-%ds %%s\n", HANDLEN + 2);
    dprintf(idx, format, buf, ln);
    free(buf);
  }
}

static void cmd_traffic(int idx, char *par)
{
  size_t itmp, itmp2;

  dprintf(idx, "Traffic since last restart\n");
  dprintf(idx, "==========================\n");
  if (traffic.out_total.irc > 0 || traffic.in_total.irc > 0 || traffic.out_today.irc > 0 ||
      traffic.in_today.irc > 0) {
    dprintf(idx, "IRC:\n");
    dprintf(idx, "  out: %s", btos(traffic.out_total.irc + traffic.out_today.irc));
              dprintf(idx, " (%s today)\n", btos(traffic.out_today.irc));
    dprintf(idx, "   in: %s", btos(traffic.in_total.irc + traffic.in_today.irc));
              dprintf(idx, " (%s today)\n", btos(traffic.in_today.irc));
  }
  if (traffic.out_total.bn > 0 || traffic.in_total.bn > 0 || traffic.out_today.bn > 0 ||
      traffic.in_today.bn > 0) {
    dprintf(idx, "Botnet:\n");
    dprintf(idx, "  out: %s", btos(traffic.out_total.bn + traffic.out_today.bn));
              dprintf(idx, " (%s today)\n", btos(traffic.out_today.bn));
    dprintf(idx, "   in: %s", btos(traffic.in_total.bn + traffic.in_today.bn));
              dprintf(idx, " (%s today)\n", btos(traffic.in_today.bn));
  }
  if (traffic.out_total.dcc > 0 || traffic.in_total.dcc > 0 || traffic.out_today.dcc > 0 ||
      traffic.in_today.dcc > 0) {
    dprintf(idx, "Partyline:\n");
    itmp = traffic.out_total.dcc + traffic.out_today.dcc;
    itmp2 = traffic.out_today.dcc;
    dprintf(idx, "  out: %s", btos(itmp));
              dprintf(idx, " (%s today)\n", btos(itmp2));
    dprintf(idx, "   in: %s", btos(traffic.in_total.dcc + traffic.in_today.dcc));
              dprintf(idx, " (%s today)\n", btos(traffic.in_today.dcc));
  }
  if (traffic.out_total.trans > 0 || traffic.in_total.trans > 0 || traffic.out_today.trans > 0 ||
      traffic.in_today.trans > 0) {
    dprintf(idx, "Transfer.mod:\n");
    dprintf(idx, "  out: %s", btos(traffic.out_total.trans + traffic.out_today.trans));
              dprintf(idx, " (%s today)\n", btos(traffic.out_today.trans));
    dprintf(idx, "   in: %s", btos(traffic.in_total.trans + traffic.in_today.trans));
              dprintf(idx, " (%s today)\n", btos(traffic.in_today.trans));
  }
  if (traffic.out_total.unknown > 0 || traffic.out_today.unknown > 0) {
    dprintf(idx, "Misc:\n");
    dprintf(idx, "  out: %s", btos(traffic.out_total.unknown + traffic.out_today.unknown));
              dprintf(idx, " (%s today)\n", btos(traffic.out_today.unknown));
    dprintf(idx, "   in: %s", btos(traffic.in_total.unknown + traffic.in_today.unknown));
              dprintf(idx, " (%s today)\n", btos(traffic.in_today.unknown));
  }
  dprintf(idx, "---\n");
  dprintf(idx, "Total:\n");
  itmp = traffic.out_total.irc + traffic.out_total.bn + traffic.out_total.dcc + traffic.out_total.trans
         + traffic.out_total.unknown + traffic.out_today.irc + traffic.out_today.bn
         + traffic.out_today.dcc + traffic.out_today.trans + traffic.out_today.unknown;
  itmp2 = traffic.out_today.irc + traffic.out_today.bn + traffic.out_today.dcc
         + traffic.out_today.trans + traffic.out_today.unknown;
  dprintf(idx, "  out: %s", btos(itmp));
              dprintf(idx, " (%s today)\n", btos(itmp2));
  dprintf(idx, "   in: %s", btos(traffic.in_total.irc + traffic.in_total.bn + traffic.in_total.dcc
	  + traffic.in_total.trans + traffic.in_total.unknown + traffic.in_today.irc
	  + traffic.in_today.bn + traffic.in_today.dcc + traffic.in_today.trans
	  + traffic.in_today.unknown));
  dprintf(idx, " (%s today)\n", btos(traffic.in_today.irc + traffic.in_today.bn
          + traffic.in_today.dcc + traffic.in_today.trans
	  + traffic.in_today.unknown));
  putlog(LOG_CMDS, "*", "#%s# traffic", dcc[idx].nick);
}

static char traffictxt[20] = "";
static char *btos(unsigned long  bytes)
{
  char unit[10] = "";
  float xbytes;

  simple_snprintf(unit, sizeof(unit), "Bytes");
  xbytes = bytes;
  if (xbytes > 1024.0) {
    simple_snprintf(unit, sizeof(unit), "KBytes");
    xbytes = xbytes / 1024.0;
  }
  if (xbytes > 1024.0) {
    simple_snprintf(unit, sizeof(unit), "MBytes");
    xbytes = xbytes / 1024.0;
  }
  if (xbytes > 1024.0) {
    simple_snprintf(unit, sizeof(unit), "GBytes");
    xbytes = xbytes / 1024.0;
  }
  if (xbytes > 1024.0) {
    simple_snprintf(unit, sizeof(unit), "TBytes");
    xbytes = xbytes / 1024.0;
  }
  if (bytes > 1024)
    egg_snprintf(traffictxt, sizeof(traffictxt), "%.2f %s", xbytes, unit);
  else
    simple_snprintf(traffictxt, sizeof(traffictxt), "%lu Bytes", bytes);
  return traffictxt;
}

static void cmd_whoami(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# whoami", dcc[idx].nick);
  dprintf(idx, "You are %s@%s.\n", dcc[idx].nick, conf.bot->nick);
}

static void cmd_quit(int idx, char *text)
{
  if (dcc[idx].simul >= 0) {
    dprintf(idx, "Sorry, that cmd isn't available over botcmd.\n");
    return;
  }

	putlog(LOG_CMDS, "*", "#%s# quit %s", dcc[idx].nick, text);

	check_bind_chof(dcc[idx].nick, idx);
	dprintf(idx, "*** Ja Mata\n");
	flush_lines(idx, dcc[idx].u.chat);
	putlog(LOG_MISC, "*", "DCC connection closed (%s!%s)", dcc[idx].nick, dcc[idx].host);
	if (dcc[idx].u.chat->channel >= 0) {
		chanout_but(-1, dcc[idx].u.chat->channel, "*** %s left the party line%s%s\n", dcc[idx].nick, text[0] ? ": " : ".", text);
		if (dcc[idx].u.chat->channel < GLOBAL_CHANS) {
			botnet_send_part_idx(idx, text);
		}
	}

	if (dcc[idx].u.chat->su_nick) {
		dcc[idx].user = get_user_by_handle(userlist, dcc[idx].u.chat->su_nick);
		strlcpy(dcc[idx].nick, dcc[idx].u.chat->su_nick, sizeof(dcc[idx].nick));
                dcc[idx].u.chat->channel = dcc[idx].u.chat->su_channel;
		dcc[idx].type = &DCC_CHAT;
		dprintf(idx, "Returning to real nick %s!\n", dcc[idx].u.chat->su_nick);
		free(dcc[idx].u.chat->su_nick);
		dcc[idx].u.chat->su_nick = NULL;

		dcc_chatter(idx);

		if (dcc[idx].u.chat->channel < GLOBAL_CHANS && dcc[idx].u.chat->channel >= 0) {
			botnet_send_join_idx(idx);
		}
	} else if ((dcc[idx].sock != STDOUT) || backgrd) {
		killsock(dcc[idx].sock);
		lostdcc(idx);
	} else {
		dprintf(DP_STDOUT, "\n### SIMULATION RESET\n\n");
		dcc_chatter(idx);
	}
}

void cmd_test(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# test", dcc[idx].nick);
}

#ifdef USE_SCRIPT_TCL
void cmd_tcl(int idx, char *par)
{
  if (!isowner(dcc[idx].nick)) {
    dprintf(idx, "tcl is only available to permanent owners.\n");
    return;
  }

  putlog(LOG_CMDS, "*", "#%s# tcl", dcc[idx].nick);

  bd::String result(tcl_eval(par));
  if (dcc[idx].irc && strcmp(dcc[idx].u.chat->con_chan, "*")) {
      privmsg(dcc[idx].u.chat->con_chan, tcl_eval(par), DP_SERVER);
  } else
    dprintf(idx, result.c_str(), DP_SERVER);
}
#endif

void cmd_botlink(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# botlink %s", dcc[idx].nick, par);

  if (!isowner(dcc[idx].nick)) {
    dprintf(idx, "botlink is only available to permanent owners.\n");
    return;
  }

  if (conf.bot->hub)
    send_hubs(par, strlen(par));
  else
    send_uplink(par, strlen(par));
}

/* DCC CHAT COMMANDS
 */
/* Function call should be:
 *   int cmd_whatever(idx,"parameters");
 * As with msg commands, function is responsible for any logging.
 */
cmd_t C_dcc[] =
{
  {"+host",		"o|o",	(Function) cmd_pls_host,	NULL, AUTH},
  {"+ignore",		"m",	(Function) cmd_pls_ignore,	NULL, AUTH},
  {"+user",		"m",	(Function) cmd_pls_user,	NULL, AUTH},
  {"-host",		"",	(Function) cmd_mns_host,	NULL, AUTH},
  {"-ignore",		"m",	(Function) cmd_mns_ignore,	NULL, AUTH},
  {"-user",		"m",	(Function) cmd_mns_user,	NULL, AUTH},
  {"addlog",		"o|o",	(Function) cmd_addlog,		NULL, AUTH},
/*  {"putlog",		"m|o",	(Function) cmd_addlog,		NULL, 0}, */
  {"about",		"",	(Function) cmd_about,		NULL, 0},
  {"addline",		"",	(Function) cmd_addline,		NULL, 0},
  {"away",		"",	(Function) cmd_away,		NULL, 0},
  {"back",		"",	(Function) cmd_back,		NULL, 0},
  {"backup",		"m|m",	(Function) cmd_backup,		NULL, HUB},
  {"boot",		"m",	(Function) cmd_boot,		NULL, HUB},
  {"bots",		"m",	(Function) cmd_bots,		NULL, HUB},
  {"downbots",		"m",	(Function) cmd_downbots,	NULL, HUB},
  {"bottree",		"n",	(Function) cmd_bottree,		NULL, HUB},
  {"chaddr",		"a",	(Function) cmd_chaddr,		NULL, HUB},
  {"chat",		"",	(Function) cmd_chat,		NULL, 0},
  {"chattr",		"m|m",	(Function) cmd_chattr,		NULL, AUTH},
  {"chhandle",		"m",	(Function) cmd_chhandle,	NULL, HUB},
/*  {"chnick",		"m",	(Function) cmd_chhandle,	NULL, HUB}, */
  {"chpass",		"m",	(Function) cmd_chpass,		NULL, HUB},
  {"chsecpass",		"n",	(Function) cmd_chsecpass,	NULL, HUB},
  {"clearhosts",	"",	(Function) cmd_clearhosts,	NULL, 0},
  {"cmdpass",           "a",    (Function) cmd_cmdpass,         NULL, HUB},
  {"color",		"",     (Function) cmd_color,           NULL, 0},
  {"comment",		"m|m",	(Function) cmd_comment,		NULL, 0},
  {"set",		"n",	(Function) cmd_set,		NULL, HUB},
  {"botset",		"n",	(Function) cmd_botset,		NULL, HUB},
  {"console",		"-|-",	(Function) cmd_console,		NULL, 0},
  {"date",		"",	(Function) cmd_date,		NULL, AUTH},
  {"dccstat",		"a",	(Function) cmd_dccstat,		NULL, 0},
  {"debug",		"a",	(Function) cmd_debug,		NULL, 0},
  {"timers",		"a",	(Function) cmd_timers,		NULL, 0},
  {"die",		"n",	(Function) cmd_die,		NULL, 0},
  {"suicide",		"a",	(Function) cmd_suicide,		NULL, 0},
  {"echo",		"",	(Function) cmd_echo,		NULL, 0},
  {"login",		"",	(Function) cmd_login,		NULL, 0},
  {"fixcodes",		"",	(Function) cmd_fixcodes,	NULL, 0},
  {"handle",		"",	(Function) cmd_handle,		NULL, AUTH},
  {"nohelp",		"-|-",	(Function) cmd_nohelp,		NULL, 0},
  {"help",		"-|-",	(Function) cmd_help,		NULL, AUTH},
  {"ignores",		"m",	(Function) cmd_ignores,		NULL, AUTH},
  {"link",		"n",	(Function) cmd_link,		NULL, HUB},
  {"match",		"m|m",	(Function) cmd_match,		NULL, 0},
  {"matchbot",		"m|m",	(Function) cmd_matchbot,	NULL, 0},
  {"me",		"",	(Function) cmd_me,		NULL, 0},
  {"motd",		"",	(Function) cmd_motd,		NULL, 0},
  {"newleaf",		"n",	(Function) cmd_newleaf,		NULL, HUB},
  {"newhub",		"a",	(Function) cmd_newhub,		NULL, HUB},
  {"nopass",		"m",	(Function) cmd_nopass,		NULL, HUB},
  {"newpass",		"",	(Function) cmd_newpass,		NULL, 0},
  {"secpass",		"",	(Function) cmd_secpass,		NULL, 0},
/*  {"nick",		"",	(Function) cmd_handle,		NULL, 0}, */
  {"page",		"",	(Function) cmd_page,		NULL, 0},
  {"quit",		"",	(Function) cmd_quit,		NULL, 0},
  {"relay",		"i",	(Function) cmd_relay,		NULL, 0},
  {"reload",		"m|m",	(Function) cmd_reload,		NULL, HUB},
  {"rehash",		"a",	(Function) cmd_rehash,		NULL, 0},
  {"restart",		"m",	(Function) cmd_restart,		NULL, 0},
  {"save",		"m|m",	(Function) cmd_save,		NULL, HUB},
  {"simul",		"a",	(Function) cmd_simul,		NULL, 0},
  {"status",		"m|m",	(Function) cmd_status,		NULL, 0},
  {"strip",		"",	(Function) cmd_strip,		NULL, 0},
  {"su",		"a",	(Function) cmd_su,		NULL, 0},
  {"trace",		"n",	(Function) cmd_trace,		NULL, HUB},
  {"traffic",		"m",	(Function) cmd_traffic,		NULL, 0},
  {"unlink",		"m",	(Function) cmd_unlink,		NULL, 0},
  {"update",		"a",	(Function) cmd_update,		NULL, 0},
  {"netcrontab",	"a",	(Function) cmd_netcrontab,	NULL, HUB},
  {"uptime",		"m|m",	(Function) cmd_uptime,		NULL, AUTH},
  {"crontab",		"a",	(Function) cmd_crontab,		NULL, 0},
  {"dns",		"",	(Function) cmd_dns,             NULL, AUTH_ALL},
  {"who",		"n",	(Function) cmd_who,		NULL, HUB},
  {"whois",		"",	(Function) cmd_whois,		NULL, AUTH},
  {"whom",		"",	(Function) cmd_whom,		NULL, 0},
  {"whoami",		"",	(Function) cmd_whoami,		NULL, AUTH},
  {"botjump",           "m",    (Function) cmd_botjump,         NULL, HUB},
  {"botmsg",		"o",    (Function) cmd_botmsg,          NULL, HUB},
  {"botnick", 		"m", 	(Function) cmd_botnick, 	NULL, HUB},
  {"netnick", 		"m", 	(Function) cmd_netnick, 	NULL, HUB},
  {"netrelease", 	"m", 	(Function) cmd_netrelease, 	NULL, HUB},
  {"netw", 		"n", 	(Function) cmd_netw, 		NULL, HUB},
  {"netps", 		"n", 	(Function) cmd_netps, 		NULL, HUB},
  {"netlast", 		"n", 	(Function) cmd_netlast, 	NULL, HUB},
  {"netlag", 		"m", 	(Function) cmd_netlag, 		NULL, HUB},
  {"botserver",		"m",	(Function) cmd_botserver,	NULL, HUB},
  {"netserver", 	"m", 	(Function) cmd_netserver, 	NULL, HUB},
  {"timesync",		"a",	(Function) cmd_timesync,	NULL, 0},
  {"botversion", 	"o", 	(Function) cmd_botversion, 	NULL, HUB},
  {"version", 		"o", 	(Function) cmd_version, 	NULL, 0},
  {"netversion", 	"o", 	(Function) cmd_netversion, 	NULL, HUB},
  {"userlist", 		"m", 	(Function) cmd_userlist, 	NULL, 0},
  {"groups", 		"m", 	(Function) cmd_groups, 		NULL, HUB},
  {"ps", 		"n", 	(Function) cmd_ps, 		NULL, 0},
  {"last", 		"n", 	(Function) cmd_last, 		NULL, 0},
  {"exec", 		"a", 	(Function) cmd_exec, 		NULL, 0},
  {"w", 		"n", 	(Function) cmd_w, 		NULL, 0},
  {"channels", 		"", 	(Function) cmd_channels, 	NULL, 0},
  {"test",		"",	(Function) cmd_test,		NULL, 0},
#ifdef USE_SCRIPT_TCL
  {"tcl",		"a",	(Function) cmd_tcl,		NULL, AUTH_ALL},
#endif
  {"botlink",		"a",	(Function) cmd_botlink,		NULL, 0},
  {"randstring", 	"", 	(Function) cmd_randstring, 	NULL, AUTH_ALL},
  {"hash",		"",	(Function) cmd_hash,		NULL, AUTH_ALL},
  {"md5",		"",	(Function) cmd_md5,		NULL, AUTH_ALL},
  {"sha1",		"",	(Function) cmd_sha1,		NULL, AUTH_ALL},
  {"sha256",		"",	(Function) cmd_sha256,		NULL, AUTH_ALL},
  {"conf",		"a",	(Function) cmd_conf,		NULL, 0},
  {"encrypt",		"",	(Function) cmd_encrypt,		NULL, AUTH_ALL},
  {"encrypt_fish",	"",	(Function) cmd_encrypt_fish,	NULL, AUTH_ALL},
  {"decrypt",		"",	(Function) cmd_decrypt,		NULL, AUTH_ALL},
  {"decrypt_fish",	"",	(Function) cmd_decrypt_fish,	NULL, AUTH_ALL},
  {"botcmd",		"i",	(Function) cmd_botcmd, 		NULL, HUB},
  {"hublevel", 		"a", 	(Function) cmd_hublevel, 	NULL, HUB},
  {"lagged", 		"m", 	(Function) cmd_lagged, 		NULL, HUB},
  {"uplink", 		"a", 	(Function) cmd_uplink, 		NULL, HUB},
  {NULL,		NULL,	NULL,				NULL, 0}
};
/* vim: set sts=2 sw=2 ts=8 et: */
