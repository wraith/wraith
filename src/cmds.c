/*
 * cmds.c -- handles:
 *   commands from a user via dcc
 *   (split in 3, this portion contains no-irc commands)
 *
 */

#include "common.h"
#include "cmds.h"
#include "settings.h"
#include "debug.h"
#include "dcc.h"
#include "shell.h"
#include "misc.h"
#include "net.h"
#include "userrec.h"
#include "users.h"
#include "egg_timer.h"
#include "userent.h"
#include "tclhash.h"
#include "match.h"
#include "main.h"
#include "dccutil.h"
#include "chanprog.h"
#include "botmsg.h"
#include "botcmd.h"	
#include "botnet.h"
#include "tandem.h"
#include "modules.h"
#include "help.h"
#include "traffic.h" /* egg_traffic_t */
#include "core_binds.h"
#include "src/mod/console.mod/console.h"
#ifdef LEAF
#include "src/mod/server.mod/server.h"
#include "src/mod/irc.mod/irc.h"
#endif /* LEAF */

#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/utsname.h>

extern egg_traffic_t 	traffic;

mycmds 			 cmdlist[500]; /* the list of dcc cmds for help system */
int    			 cmdi = 0;

static char		 *btos(unsigned long);

#ifdef HUB
static void tell_who(struct userrec *u, int idx, int chan)
{
  int i, k, ok = 0, atr = u ? u->flags : 0;
  int nicklen;
  char format[81] = "";
#ifdef HUB
  char s[1024] = "";
#endif /* HUB */

  if (!chan)
    dprintf(idx, "%s  (* = %s, + = %s, @ = %s)\n", BOT_PARTYMEMBS, MISC_OWNER, MISC_MASTER, MISC_OP);
  else {
      dprintf(idx, "%s %s%d:  (* = %s, + = %s, @ = %s)\n",
                      BOT_PEOPLEONCHAN,
                      (chan < GLOBAL_CHANS) ? "" : "*",
                      chan % GLOBAL_CHANS,
                      MISC_OWNER, MISC_MASTER, MISC_OP);
  }

  /* calculate max nicklen */
  nicklen = 0;
  for (i = 0; i < dcc_total; i++) {
      if(strlen(dcc[i].nick) > nicklen)
          nicklen = strlen(dcc[i].nick);
  }
  if(nicklen < 9) nicklen = 9;
  
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type == &DCC_CHAT)
      if (dcc[i].u.chat->channel == chan) {
	if (atr & USER_OWNER) {
	  egg_snprintf(format, sizeof format, "  [%%.2lu]  %%c%%-%us %%s", nicklen);
	  sprintf(s, format,
		  dcc[i].sock, (geticon(i) == '-' ? ' ' : geticon(i)),
		  dcc[i].nick, dcc[i].host);
	} else {
	  egg_snprintf(format, sizeof format, "  %%c%%-%us %%s", nicklen);
	  sprintf(s, format,
		  (geticon(i) == '-' ? ' ' : geticon(i)),
		  dcc[i].nick, dcc[i].host);
	}
	if (atr & USER_MASTER) {
	  if (dcc[i].u.chat->con_flags)
	    sprintf(&s[strlen(s)], " (con:%s)",
		    masktype(dcc[i].u.chat->con_flags));
	}
	if (now - dcc[i].timeval > 300) {
	  unsigned long days, hrs, mins;

	  days = (now - dcc[i].timeval) / 86400;
	  hrs = ((now - dcc[i].timeval) - (days * 86400)) / 3600;
	  mins = ((now - dcc[i].timeval) - (hrs * 3600)) / 60;
	  if (days > 0)
	    sprintf(&s[strlen(s)], " (idle %lud%luh)", days, hrs);
	  else if (hrs > 0)
	    sprintf(&s[strlen(s)], " (idle %luh%lum)", hrs, mins);
	  else
	    sprintf(&s[strlen(s)], " (idle %lum)", mins);
	}
	dprintf(idx, "%s\n", s);
	if (dcc[i].u.chat->away != NULL)
	  dprintf(idx, "      AWAY: %s\n", dcc[i].u.chat->away);
      }
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type == &DCC_BOT) {
      if (!ok) {
	ok = 1;
	dprintf(idx, STR("Bots connected:\n"));
      }
#ifdef S_UTCTIME
      egg_strftime(s, 14, "%d %b %H:%M %Z", gmtime(&dcc[i].timeval));
#else /* !S_UTCTIME */
      egg_strftime(s, 14, "%d %b %H:%M %Z", localtime(&dcc[i].timeval));
#endif /* S_UTCTIME */
      if (atr & USER_OWNER) {
        egg_snprintf(format, sizeof format, "  [%%.2lu]  %%s%%c%%-%us (%%s) %%s\n", 
			    nicklen);
	dprintf(idx, format,
		dcc[i].sock, dcc[i].status & STAT_CALLED ? "<-" : "->",
		dcc[i].status & STAT_SHARE ? '+' : ' ',
		dcc[i].nick, s, dcc[i].u.bot->version);
      } else {
        egg_snprintf(format, sizeof format, "  %%s%%c%%-%us (%%s) %%s\n", nicklen);
	dprintf(idx, format,
		dcc[i].status & STAT_CALLED ? "<-" : "->",
		dcc[i].status & STAT_SHARE ? '+' : ' ',
		dcc[i].nick, s, dcc[i].u.bot->version);
      }
    }
  ok = 0;
  for (i = 0; i < dcc_total; i++) {
    if ((dcc[i].type == &DCC_CHAT) && (dcc[i].u.chat->channel != chan)) {
      if (!ok) {
	ok = 1;
	dprintf(idx, STR("Other people on the bot:\n"));
      }
      if (atr & USER_OWNER) {
	egg_snprintf(format, sizeof format, "  [%%.2lu]  %%c%%-%us ", nicklen);
	sprintf(s, format, dcc[i].sock,
		(geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick);
      } else {
	egg_snprintf(format, sizeof format, "  %%c%%-%us ", nicklen);
	sprintf(s, format,
		(geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick);
      }
      if (atr & USER_MASTER) {
	if (dcc[i].u.chat->channel < 0)
	  strcat(s, "(-OFF-) ");
	else if (!dcc[i].u.chat->channel)
	  strcat(s, "(party) ");
	else
	  sprintf(&s[strlen(s)], "(%5d) ", dcc[i].u.chat->channel);
      }
      strcat(s, dcc[i].host);
      if (atr & USER_MASTER) {
	if (dcc[i].u.chat->con_flags)
	  sprintf(&s[strlen(s)], " (con:%s)",
		  masktype(dcc[i].u.chat->con_flags));
      }
      if (now - dcc[i].timeval > 300) {
	k = (now - dcc[i].timeval) / 60;
	if (k < 60)
	  sprintf(&s[strlen(s)], " (idle %dm)", k);
	else
	  sprintf(&s[strlen(s)], " (idle %dh%dm)", k / 60, k % 60);
      }
      dprintf(idx, "%s\n", s);
      if (dcc[i].u.chat->away != NULL)
	dprintf(idx, "      AWAY: %s\n", dcc[i].u.chat->away);
    }
    if ((atr & USER_MASTER) && (dcc[i].type->flags & DCT_SHOWWHO) &&
	(dcc[i].type != &DCC_CHAT)) {
      if (!ok) {
	ok = 1;
	dprintf(idx, STR("Other people on the bot:\n"));
      }
      if (atr & USER_OWNER) {
	egg_snprintf(format, sizeof format, "  [%%.2lu]  %%c%%-%us (files) %%s", 
				nicklen);
	sprintf(s, format,
		dcc[i].sock, dcc[i].status & STAT_CHAT ? '+' : ' ',
		dcc[i].nick, dcc[i].host);
      } else {
	egg_snprintf(format, sizeof format, "  %%c%%-%us (files) %%s", nicklen);
	sprintf(s, format,
		dcc[i].status & STAT_CHAT ? '+' : ' ',
		dcc[i].nick, dcc[i].host);
      }
      dprintf(idx, "%s\n", s);
    }
  }
}

static void cmd_botinfo(struct userrec *u, int idx, char *par)
{
  char s[512] = "", s2[32] = "";
  time_t now2;
  int hr, min;

  putlog(LOG_CMDS, "*", STR("#%s# botinfo"), dcc[idx].nick);

  now2 = now - online_since;
  s2[0] = 0;
  if (now2 > 86400) {
    int days = now2 / 86400;

    /* Days */
    sprintf(s2, "%d day", days);
    if (days >= 2)
      strcat(s2, "s");
    strcat(s2, ", ");
    now2 -= days * 86400;
  }
  hr = (time_t) ((int) now2 / 3600);
  now2 -= (hr * 3600);
  min = (time_t) ((int) now2 / 60);
  sprintf(&s2[strlen(s2)], "%02d:%02d", (int) hr, (int) min);
  simple_sprintf(s, "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, conf.bot->nick);
  botnet_send_infoq(-1, s);

  dprintf(idx, STR("*** [%s] %s <NO_IRC> [UP %s]\n"), conf.bot->nick, ver, s2);
}
#endif /* HUB */

static void cmd_whom(struct userrec *u, int idx, char *par)
{
  if (par[0] == '*') {
    putlog(LOG_CMDS, "*", STR("#%s# whom %s"), dcc[idx].nick, par);
    answer_local_whom(idx, -1);
    return;
  } else if (dcc[idx].u.chat->channel < 0) {
    dprintf(idx, STR("You have chat turned off.\n"));
    return;
  }
  putlog(LOG_CMDS, "*", STR("#%s# whom %s"), dcc[idx].nick, par);
  if (!par[0]) {
    answer_local_whom(idx, dcc[idx].u.chat->channel);
  } else {
    int chan = -1;

    if ((par[0] < '0') || (par[0] > '9')) {
      if (chan <= 0) {
	dprintf(idx, STR("No such channel exists.\n"));
	return;
      }
    } else
      chan = atoi(par);
    if ((chan < 0) || (chan > 99999)) {
      dprintf(idx, STR("Channel number out of range: must be between 0 and 99999.\n"));
      return;
    }
    answer_local_whom(idx, chan);
  }
}

#ifdef HUB
/*
   .config
   Usage + available entry list
   .config name
   Show current value + description
   .config name value
   Set
 */
static void cmd_config(struct userrec *u, int idx, char *par)
{
  char *name = NULL;
  struct cfg_entry *cfgent = NULL;
  int cnt, i;

  putlog(LOG_CMDS, "*", STR("#%s# config %s"), dcc[idx].nick, par);
  if (!par[0]) {
    char *outbuf = NULL;

    outbuf = malloc(1);

    dprintf(idx, STR("Usage: config [name [value|-]]\n"));
    dprintf(idx, STR("Defined config entry names:\n"));
    cnt = 0;
    for (i = 0; i < cfg_count; i++) {
      if ((cfg[i]->flags & CFGF_GLOBAL) && (cfg[i]->describe)) {
	if (!cnt) {
          outbuf = realloc(outbuf, 2 + 1);
	  sprintf(outbuf, "  ");
        }
        outbuf = realloc(outbuf, strlen(outbuf) + strlen(cfg[i]->name) + 1 + 1);
	sprintf(outbuf, STR("%s%s "), outbuf, cfg[i]->name);
	cnt++;
	if (cnt == 10) {
	  dprintf(idx, "%s\n", outbuf);
	  cnt = 0;
	}
      }
    }
    if (cnt)
      dprintf(idx, "%s\n", outbuf);
    if (outbuf)
      free(outbuf);
    return;
  }
  name = newsplit(&par);
  for (i = 0; !cfgent && (i < cfg_count); i++)
    if (!strcmp(cfg[i]->name, name))
      cfgent = cfg[i];
  if (!cfgent || !cfgent->describe) {
    dprintf(idx, STR("No such config entry\n"));
    return;
  }
  if (!par[0]) {
    cfgent->describe(cfgent, idx);
    if (!cfgent->gdata)
      dprintf(idx, STR("No current value\n"));
    else {
      dprintf(idx, STR("Currently: %s\n"), cfgent->gdata);
    }
    return;
  }
  if (strlen(par) >= 2048) {
    dprintf(idx, STR("Value can't be longer than 2048 chars"));
    return;
  }
  set_cfg_str(NULL, cfgent->name, par);
  if (!cfgent->gdata)
    dprintf(idx, STR("Now: (not set)\n"));
  else {
    dprintf(idx, STR("Now: %s\n"), cfgent->gdata);
  }
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
}

static void cmd_botconfig(struct userrec *u, int idx, char *par)
{
  struct userrec *u2 = NULL;
  char *p = NULL;
  struct xtra_key *k = NULL;
  struct cfg_entry *cfgent = NULL;
  int i, cnt;

  /* botconfig bot [name [value]]  */
  putlog(LOG_CMDS, "*", STR("#%s# botconfig %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: botconfig bot [name [value|-]]\n"));
    cnt = 0;
    for (i = 0; i < cfg_count; i++) {
      if (cfg[i]->flags & CFGF_LOCAL) {
	dprintf(idx, STR("%s "), cfg[i]->name);
	cnt++;
	if (cnt == 10) {
	  dprintf(idx, "\n");
	  cnt=0;
	}
      }
    }
    if (cnt > 0)
      dprintf(idx, "\n");
    return;
  }
  p = newsplit(&par);
  u2 = get_user_by_handle(userlist, p);
  if (!u2) {
    dprintf(idx, STR("No such user.\n"));
    return;
  }
  if (!(u2->flags & USER_BOT)) {
    dprintf(idx, STR("%s isn't a bot.\n"), p);
    return;
  }
  if (!par[0]) {
    for (i = 0; i < cfg_count; i++) {
      if ((cfg[i]->flags & CFGF_LOCAL) && (cfg[i]->describe)) {
	k = get_user(&USERENTRY_CONFIG, u2);
	while (k && strcmp(k->key, cfg[i]->name))
	  k=k->next;
	if (k)
	  dprintf(idx, "  %s: %s\n", k->key, k->data);
	else
	  dprintf(idx, "  %s: (not set)\n", cfg[i]->name);
      }
    }
    return;
  }
  p = newsplit(&par);
  cfgent = NULL;
  for (i = 0; !cfgent && (i < cfg_count); i++)
    if (!strcmp(cfg[i]->name, p) && (cfg[i]->flags & CFGF_LOCAL) && (cfg[i]->describe))
      cfgent=cfg[i];
  if (!cfgent) {
    dprintf(idx, "No such configuration value\n");
    return;
  }
  if (par[0]) {
    char tmp[100] = "";

    set_cfg_str(u2->handle, cfgent->name, (strcmp(par, "-")) ? par : NULL);
    egg_snprintf(tmp, sizeof tmp, "%s %s", cfgent->name, par);
    update_mod(u2->handle, dcc[idx].nick, "botconfig", tmp);
    dprintf(idx, "Now: ");
#ifdef HUB
    write_userfile(idx);
#endif /* HUB */
  } else {
    if (cfgent->describe)
      cfgent->describe(cfgent, idx);
  }
  k = get_user(&USERENTRY_CONFIG, u2);
  while (k && strcmp(k->key, cfgent->name))
    k = k->next;
  if (k)
    dprintf(idx, STR("  %s: %s\n"), k->key, k->data);
  else
    dprintf(idx, STR("  %s: (not set)\n"), cfgent->name);
}

#ifdef S_DCCPASS
static void cmd_cmdpass(struct userrec *u, int idx, char *par)
{
  bind_entry_t *entry = NULL;
  bind_table_t *table = NULL;
  char *cmd = NULL, *pass = NULL;
  int i, l = 0;

  /* cmdpass [command [newpass]] */
  table = bind_table_lookup("dcc");
  cmd = newsplit(&par);
  putlog(LOG_CMDS, "*", STR("#%s# cmdpass %s ..."), dcc[idx].nick, cmd[0] ? cmd : "");
  pass = newsplit(&par);
  if (!cmd[0] || par[0]) {
    dprintf(idx, STR("Usage: %scmdpass <command> [password]\n"), dcc_prefix);
    dprintf(idx, STR("  if no password is specified, the commands password is reset\n"));
    return;
  }
  for (i = 0; cmd[i]; i++)
    cmd[i] = tolower(cmd[i]);

  /* need to check for leaf cmds, and set l > 0 here */

  if (!l) {
    int found = 0;

    for (entry = table->entries; entry && entry->next; entry = entry->next) {
      if (!egg_strcasecmp(cmd, entry->mask)) {
        found++;
        break;
      }
    }

    if (!found) {
      dprintf(idx, STR("No such DCC command\n"));
      return;
    }
  }

  if (pass[0]) {
    char epass[36] = "", tmp[256] = "";

    if (!isowner(u->handle) && has_cmd_pass(cmd)) {
      putlog(LOG_MISC, "*", STR("%s attempted to change command password for %s - not perm owner"), dcc[idx].nick, cmd);
      dprintf(idx, STR("Perm owners only.\n"));
      return;
    }
    encrypt_pass(pass, epass);
    sprintf(tmp, STR("%s %s"), cmd, epass);
    if (has_cmd_pass(cmd))
      dprintf(idx, STR("Changed command password for %s\n"), cmd);
    else
      dprintf(idx, STR("Set command password for %s to '%s'\n"), cmd, pass);
    set_cmd_pass(tmp, 1);
  } else {
    if (!isowner(u->handle)) {
      putlog(LOG_MISC, "*", STR("%s attempted to remove command password for %s - not perm owner"), dcc[idx].nick, cmd);
      dprintf(idx, STR("Perm owners only.\n"));
      return;
    }
    set_cmd_pass(cmd, 1);
    dprintf(idx, STR("Removed command password for %s\n"), cmd);
  }
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
}
#endif /* S_DCCPASS */

static void cmd_lagged(struct userrec *u, int idx, char *par)
{
  /* Lists botnet lag to *directly connected* bots */
  int i;

  putlog(LOG_CMDS, "*", STR("#%s# lagged %s"), u->handle, par);
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type == &DCC_BOT) {
      dprintf(idx, STR("%9s - %i seconds\n"), dcc[i].nick, (dcc[i].pingtime > 120) ? (now - dcc[i].pingtime) : dcc[i].pingtime);
    }
  }
}
#endif /* HUB */

static void cmd_me(struct userrec *u, int idx, char *par)
{
  int i;

  if (dcc[idx].u.chat->channel < 0) {
    dprintf(idx, STR("You have chat turned off.\n"));
    return;
  }
  if (!par[0]) {
    dprintf(idx, STR("Usage: me <action>\n"));
    return;
  }
  if (dcc[idx].u.chat->away != NULL)
    not_away(idx);
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type->flags & DCT_CHAT) &&
	(dcc[i].u.chat->channel == dcc[idx].u.chat->channel) &&
	((i != idx) || (dcc[i].status & STAT_ECHO)))
      dprintf(i, "* %s %s\n", dcc[idx].nick, par);
  botnet_send_act(idx, conf.bot->nick, dcc[idx].nick,
		  dcc[idx].u.chat->channel, par);
  check_bind_act(dcc[idx].nick, dcc[idx].u.chat->channel, par);
}

static void cmd_motd(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# motd %s"), dcc[idx].nick, par);
  if (par[0] && (u->flags & USER_MASTER)) {
    char *s = NULL;

    s = malloc(strlen(par) + 1 + strlen(dcc[idx].nick) + 10 + 1 + 1); /* +2: ' 'x2 */

    sprintf(s, "%s %lu %s", dcc[idx].nick, now, par);
    set_cfg_str(NULL, "motd", s);
    free(s);
    dprintf(idx, "Motd set\n");
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
  } else {
    show_motd(idx);
  }
}

static void cmd_about(struct userrec *u, int idx, char *par)
{
  char c[80] = "";

  putlog(LOG_CMDS, "*", STR("#%s# about"), dcc[idx].nick);
  dprintf(idx, STR("Wraith botpack by bryan\n"));
  egg_strftime(c, sizeof c, "%c %Z", gmtime(&buildts));
  dprintf(idx, STR("Version: %s\n"), egg_version);
  dprintf(idx, STR("Build: %s (%lu)\n"), c, buildts);
  dprintf(idx, STR("(written from a base of Eggdrop 1.6.12)\n"));
  dprintf(idx, STR("..with credits and thanks to the following:\n"));
  dprintf(idx, STR(" \n"));
  dprintf(idx, STR(" * Eggdrop team for developing such a great bot to code off of.\n"));
  dprintf(idx, STR(" * $bEinride$b and $bievil$b for taking eggdrop1.4.3 and making their very effecient botpack Ghost.\n"));
  dprintf(idx, STR(" * $bryguy$b for beta testing, providing code, finding bugs, and providing input.\n"));
  dprintf(idx, STR(" * $bSFC$b for providing compile shells, continuous input, feature suggestions, and testing.\n"));
  dprintf(idx, STR(" * $bxmage$b for beta testing.\n"));
  dprintf(idx, STR(" * $bpasswd$b for beta testing, and his dedication to finding bugs.\n"));
  dprintf(idx, STR(" * $bextort$b for finding misc bugs.\n"));
  dprintf(idx, STR(" * $bpgpkeys$b for finding bugs, and providing input.\n"));
  dprintf(idx, STR(" * $bqFox$b for providing an mIRC $md5() alias, not requiring a dll or >6.03\n"));
  dprintf(idx, STR(" * $bSith_Lord$b helping test ipv6 on the bot (admin@elitepackets.com)\n"));
  dprintf(idx, STR(" * $bExcelsior$b for finding a bug on BSD with the ipv6, and for celdrop which inspired many features.\n"));
  dprintf(idx, STR(" * $bsyt$b for giving me inspiration to code a more secure bot.\n"));
  dprintf(idx, STR(" * $Blackjac$b for helping with the bx auth script with his Sentinel script.\n"));
  dprintf(idx, STR(" * $bMystikal$b for various bugs\n"));
  dprintf(idx, STR(" \n"));
  dprintf(idx, STR("The following botpacks gave me inspiration and ideas (no code):\n"));
  dprintf(idx, STR(" * $uawptic$u by $blordoptic$b\n"));
  dprintf(idx, STR(" * $uoptikz$u by $bryguy$b and $blordoptic$b\n"));
  dprintf(idx, STR(" * $uceldrop$u by $bexcelsior$b\n"));
  dprintf(idx, STR(" * $ugenocide$u by $Crazi$b, $bDor$b, $bpsychoid$b, and $bAce24$b\n"));
  dprintf(idx, STR(" * $utfbot$u by $bwarknite$b and $bloslinux$b\n"));
}

static void cmd_away(struct userrec *u, int idx, char *par)
{
  if (strlen(par) > 60)
    par[60] = 0;
  set_away(idx, par);
}

static void cmd_back(struct userrec *u, int idx, char *par)
{
  not_away(idx);
}

static void cmd_newpass(struct userrec *u, int idx, char *par)
{
  char *new = NULL, pass[16] = "";

  putlog(LOG_CMDS, "*", STR("#%s# newpass..."), dcc[idx].nick);
  if (!par[0]) {
    dprintf(idx, STR("Usage: newpass <newpassword>\n"));
    return;
  }
  new = newsplit(&par);

  if (!strcmp(new, "rand")) {
    make_rand_str(pass, 15);
  } else {
    if (strlen(new) < 6) {
      dprintf(idx, STR("Please use at least 6 characters.\n"));
      return;
    } else {
      sprintf(pass, "%s", new);
    }
  }
  if (strlen(pass) > 15)
    pass[15] = 0;

  if (!goodpass(pass, idx, NULL))
    return;

  set_user(&USERENTRY_PASS, u, pass);
  dprintf(idx, STR("Changed your password to: %s\n"), pass);
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
}

static void cmd_secpass(struct userrec *u, int idx, char *par)
{
  char *new = NULL, pass[16] = "";

  putlog(LOG_CMDS, "*", STR("#%s# secpass..."), dcc[idx].nick);
  if (!par[0]) {
    dprintf(idx, STR("Usage: secpass <newsecpass>\nIf you use \"rand\" as the secpass, a random pass will be chosen.\n"));
    return;
  }
  new = newsplit(&par);

  if (!strcmp(new, "rand")) {
    make_rand_str(pass, 15);
  } else {
    if (strlen(new) < 6) {
      dprintf(idx, STR("Please use at least 6 characters.\n"));
      return;
    } else {
      sprintf(pass, "%s", new);
    }
  }
  if (strlen(pass) > 15)
    pass[15] = 0;
  set_user(&USERENTRY_SECPASS, u, pass);
  dprintf(idx, STR("Changed your secpass to: %s\n"), pass);
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
}

#ifdef HUB
static void cmd_bots(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# bots"), dcc[idx].nick);
  tell_bots(idx);
}

static void cmd_downbots(struct userrec *u, int idx, char *par)
{
  struct userrec *u2 = NULL;
  int cnt = 0, tot = 0;
  char work[128] = "";

  putlog(LOG_CMDS, "*", STR("#%s# downbots"), dcc[idx].nick);
  for (u2 = userlist; u2; u2 = u2->next) {
    if (u2->flags & USER_BOT) {
      if (egg_strcasecmp(u2->handle, conf.bot->nick)) {
        if (nextbot(u2->handle) == -1) {
          strcat(work, u2->handle);
          cnt++;
          tot++;
          if (cnt == 10) {
            dprintf(idx, STR("Down bots: %s\n"), work);
            work[0] = 0;
            cnt = 0;
          } else
            strcat(work, " ");
        }
      }
    }
  }
  if (work[0])
    dprintf(idx, STR("Down bots: %s\n"), work);
  dprintf(idx, STR("(Total down: %d)\n"), tot);
}


static void cmd_bottree(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# bottree"), dcc[idx].nick);
  tell_bottree(idx, 0);
}

static void cmd_vbottree(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# vbottree"), dcc[idx].nick);
  tell_bottree(idx, 1);
}
#endif /* HUB */

int my_cmp (const mycmds *c1, const mycmds *c2)
{
  return strcmp (c1->name, c2->name);
}

static void cmd_nohelp(struct userrec *u, int idx, char *par)
{
  int i;
  char *buf = NULL;

  buf = calloc(1, 1);

  qsort(cmdlist, cmdi, sizeof(mycmds), (int (*)()) &my_cmp);
  
  for (i = 0; i < cmdi; i++) {
    int o, found = 0;
    for (o = 0; (help[o].cmd) && (help[o].desc); o++)
      if (!egg_strcasecmp(help[o].cmd, cmdlist[i].name)) found++;
    if (!found) {
      buf = realloc(buf, strlen(buf) + 2 + strlen(cmdlist[i].name) + 1);
      strcat(buf, cmdlist[i].name);
      strcat(buf, ", ");
    }
  }
  buf[strlen(buf) - 1] = 0;

  dumplots(idx, "", buf);

}

static void cmd_help(struct userrec *u, int idx, char *par)
{
  char flg[100] = "", *fcats = NULL, temp[100] = "", buf[2046] = "", match[20] = "";
  int fnd = 0, done = 0, nowild = 0;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};

  egg_snprintf(temp, sizeof temp, "a|- a|a n|- n|n m|- m|m mo|o m|o i|- o|o o|- p|- -|-");
  fcats = temp;

  putlog(LOG_CMDS, "*", STR("#%s# help %s"), dcc[idx].nick, par);
  get_user_flagrec(u, &fr, dcc[idx].u.chat->con_chan);
  build_flags(flg, &fr, NULL);
  if (!par[0]) {
    sprintf(match, "*");
  } else {
    if (!strchr(par, '*') && !strchr(par, '?'))
      nowild++;
    sprintf(match, "%s", newsplit(&par));
  }
  if (!nowild)
    dprintf(idx, STR("Showing help topics matching '%s' for flags: (%s)\n"), match, flg);

  qsort(cmdlist, cmdi, sizeof(mycmds), (int (*)()) &my_cmp);

  /* even if we have nowild, we loop to conserve code/space */
  while (!done) {
    int i = 0, end = 0, first = 1, n, hi;
    char *flag = NULL;

    flag = newsplit(&fcats);
    if (!flag[0]) 
      done = 1;

    for (n = 0; n < cmdi; n++) { /* loop each command */
      if (!flagrec_ok(&cmdlist[n].flags, &fr) || !wild_match(match, cmdlist[n].name))
        continue;
      fnd++;
      if (nowild) {
        flg[0] = 0;
        build_flags(flg, &(cmdlist[n].flags), NULL);
        dprintf(idx, STR("Showing you help for '%s' (%s):\n"), match, flg);
        for (hi = 0; (help[hi].cmd) && (help[hi].desc); hi++) {
          if (!egg_strcasecmp(match, help[hi].cmd)) {
            if (help[hi].garble)
              showhelp(idx, &fr, degarble(help[hi].garble, help[hi].desc));
            else
              showhelp(idx, &fr, help[hi].desc);
          }
        }
        done = 1;
        break;
      } else {
        flg[0] = 0;
        build_flags(flg, &(cmdlist[n].flags), NULL);
        if (!strcmp(flg, flag)) {
          if (first) {
            dprintf(idx, "%s\n", buf[0] ? buf : "");
            dprintf(idx, STR("# DCC (%s)\n"), flag);
            sprintf(buf, "  ");
          }
          if (end && !first) {
            dprintf(idx, "%s\n", buf[0] ? buf : "");
            /* we dumped the buf to dprintf, now start a new one... */
            sprintf(buf, "  ");
          }
          sprintf(buf, "%s%-14.14s", buf[0] ? buf : "", cmdlist[n].name);
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
  if (buf && buf[0])
    dprintf(idx, "%s\n", buf);
  if (fnd) 
    dprintf(idx, STR("--End help listing\n"));
  if (!strcmp(match, "*")) {
    dprintf(idx, STR("For individual command help, type: %shelp <command>\n"), dcc_prefix);
  } else if (!fnd) {
    dprintf(idx, STR("No match for '%s'.\n"), match);
  }
  dprintf(idx, STR("If you have flags on a channel, type %sconsole #chan to possibly see more commands.\n"), dcc_prefix);
}

static void cmd_addlog(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# addlog %s"), dcc[idx].nick, par);
  putlog(LOG_MISC, "*", "%s: %s", dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: addlog <message>\n"));
    return;
  }
  dprintf(idx, STR("Placed entry in the log file.\n"));
}

#ifdef HUB
static void cmd_who(struct userrec *u, int idx, char *par)
{
  int i;

  if (par[0]) {
    if (dcc[idx].u.chat->channel < 0) {
      dprintf(idx, STR("You have chat turned off.\n"));
      return;
    }
    putlog(LOG_CMDS, "*", STR("#%s# who %s"), dcc[idx].nick, par);
    if (!egg_strcasecmp(par, conf.bot->nick))
      tell_who(u, idx, dcc[idx].u.chat->channel);
    else {
      i = nextbot(par);
      if (i < 0) {
	dprintf(idx, STR("That bot isn't connected.\n"));
      } else if (dcc[idx].u.chat->channel > 99999)
	dprintf(idx, STR("You are on a local channel.\n"));
      else {
	char s[40] = "";

	simple_sprintf(s, "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, conf.bot->nick);
	botnet_send_who(i, s, par, dcc[idx].u.chat->channel);
      }
    }
  } else {
    putlog(LOG_CMDS, "*", STR("#%s# who"), dcc[idx].nick);
    if (dcc[idx].u.chat->channel < 0)
      tell_who(u, idx, 0);
    else
      tell_who(u, idx, dcc[idx].u.chat->channel);
  }
}
#endif /* HUB */

static void cmd_whois(struct userrec *u, int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, STR("Usage: whois <handle>\n"));
    return;
  }
  putlog(LOG_CMDS, "*", STR("#%s# whois %s"), dcc[idx].nick, par);
  tell_user_ident(idx, par, u ? (u->flags & USER_MASTER) : 0);
}

static void cmd_match(struct userrec *u, int idx, char *par)
{
  int start = 1, limit = 20;
  char *s = NULL, *s1 = NULL, *chname = NULL;

  if (!par[0]) {
    dprintf(idx, STR("Usage: match <nick/host> [[skip] count]\n"));
    return;
  }
  putlog(LOG_CMDS, "*", STR("#%s# match %s"), dcc[idx].nick, par);
  s = newsplit(&par);
  if (strchr(CHANMETA, par[0]) != NULL)
    chname = newsplit(&par);
  else
    chname = "";
  if (atoi(par) > 0) {
    s1 = newsplit(&par);
    if (atoi(par) > 0) {
      start = atoi(s1);
      limit = atoi(par);
    } else
      limit = atoi(s1);
  }
  tell_users_match(idx, s, start, limit, u ? (u->flags & USER_MASTER) : 0,
		   chname);
}

static void cmd_update(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# update %s"), dcc[idx].nick, par);
#ifdef LEAF
  if (!localhub)
    dprintf(idx, STR("Please use this command on the first listed in the conf for this user@box\n"));
#endif /* LEAF */
  if (!par[0])
    dprintf(idx, STR("Usage: update <binname>\n"));
  updatebin(idx, par, 0);
}

static void cmd_uptime(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# uptime"), dcc[idx].nick);
  tell_verbose_uptime(idx);
}

static void cmd_userlist(struct userrec *u, int idx, char *par)
{
  int cnt = 0, tt = 0;

  putlog(LOG_CMDS, "*", STR("#%s# userlist"), dcc[idx].nick);

  for (u = userlist; u; u = u->next) {
    if (whois_access(dcc[idx].user, u) && (u->flags & USER_BOT) && (u->flags & USER_CHANHUB)) {
      if (cnt)
        dprintf(idx, ", ");
      else
        dprintf(idx, STR("Chathubs: "));
      dprintf(idx, u->handle);
      cnt++;
      tt++;
      if (cnt == 15) {
        dprintf(idx, "\n");
        cnt=0;
      }
    }
  }

  if (cnt)
    dprintf(idx, "\n");
  cnt = 0;

#ifdef HUB
  for (u = userlist; u; u = u->next) {
    if (whois_access(dcc[idx].user, u) && !(u->flags & USER_BOT) && (u->flags & USER_ADMIN)) {
      if (cnt)
        dprintf(idx, ", ");
      else
        dprintf(idx, STR("Admins  : "));
      dprintf(idx, u->handle);
      cnt++;
      tt++;
      if (cnt == 15) {
        dprintf(idx, "\n");
        cnt = 0;
      }
    }
  }

  if (cnt)
    dprintf(idx, "\n");
  cnt = 0;


  for (u = userlist; u; u = u->next) {
    if (whois_access(dcc[idx].user, u) && !(u->flags & (USER_BOT | USER_ADMIN)) && (u->flags & USER_OWNER)) {
      if (cnt)
        dprintf(idx, ", ");
      else
        dprintf(idx, STR("Owners  : "));
      dprintf(idx, u->handle);
      cnt++;
      tt++;
      if (cnt == 15) {
        dprintf(idx, "\n");
        cnt = 0;
      }
    }
  }

  if (cnt)
    dprintf(idx, "\n");
  cnt = 0;

  for (u = userlist; u; u = u->next) {
    if (whois_access(dcc[idx].user, u) && !(u->flags & (USER_BOT | USER_OWNER)) && (u->flags & USER_MASTER)) {
      if (cnt)
        dprintf(idx, ", ");
      else
        dprintf(idx, STR("Masters : "));
      dprintf(idx, u->handle);
      cnt++;
      tt++;
      if (cnt == 15) {
        dprintf(idx, "\n");
        cnt = 0;
      }
    }
  }
  if (cnt)
    dprintf(idx, "\n");
  cnt = 0;
#endif /* HUB */

  for (u = userlist; u; u = u->next) {
    if (whois_access(dcc[idx].user, u)) {
#ifdef HUB
      if (!(u->flags & (USER_BOT | USER_MASTER)) && (u->flags & USER_OP)) {
#else /* !HUB */
      if (!(u->flags & USER_BOT) && (u->flags & USER_OP)) {
#endif /* HUB */
        if (cnt)
          dprintf(idx, ", ");
        else
          dprintf(idx, STR("Ops     : "));
        dprintf(idx, u->handle);
        cnt++;
        tt++;
        if (cnt == 15) {
          dprintf(idx, "\n");
          cnt= 0 ;
        }
      }
    }
  }
  if (cnt)
    dprintf(idx, "\n");
  cnt = 0;

  for (u = userlist; u; u = u->next) {
    if (whois_access(dcc[idx].user, u) && !(u->flags & (USER_BOT | USER_OP))) {
      if (cnt)
        dprintf(idx, ", ");
      else
        dprintf(idx, STR("Users   : "));
      dprintf(idx, u->handle);
      cnt++;
      tt++;
      if (cnt==15) {
        dprintf(idx, "\n");
        cnt = 0;
      }
    }
  }
  if (cnt)
    dprintf(idx, "\n");
  cnt = 0;
  dprintf(idx, STR("Total users: %d\n"), tt);
}

static void cmd_channels(struct userrec *u, int idx, char *par) {
  putlog(LOG_CMDS, "*", STR("#%s# channels %s"), dcc[idx].nick, par);
  if (par[0] && (u->flags & USER_MASTER)) {
    struct userrec *user = NULL;

    user = get_user_by_handle(userlist, par);
    if (user && whois_access(u, user)) {
      show_channels(idx, par);
    } else  {
      dprintf(idx, STR("There is no user by that name.\n"));
    }
  } else {
      show_channels(idx, NULL);
  }

  if ((u->flags & USER_MASTER) && !(par && par[0]))
    dprintf(idx, STR("You can also %schannels <user>\n"), dcc_prefix);
}


static void cmd_status(struct userrec *u, int idx, char *par)
{
  int atr = u ? u->flags : 0;

  if (!egg_strcasecmp(par, "all")) {
    if (!(atr & USER_MASTER)) {
      dprintf(idx, STR("You do not have Bot Master privileges.\n"));
      return;
    }
    putlog(LOG_CMDS, "*", STR("#%s# status all"), dcc[idx].nick);
    tell_verbose_status(idx);
    dprintf(idx, "\n");
    tell_settings(idx);
    do_module_report(idx, 1, NULL);
  } else {
    putlog(LOG_CMDS, "*", STR("#%s# status"), dcc[idx].nick);
    tell_verbose_status(idx);
    do_module_report(idx, 0, NULL);
  }
}

static void cmd_dccstat(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# dccstat"), dcc[idx].nick);
  tell_dcc(idx);
}

static void cmd_boot(struct userrec *u, int idx, char *par)
{
  char *who = NULL;
  int i, ok = 0;
  struct userrec *u2 = NULL;

  if (!par[0]) {
    dprintf(idx, STR("Usage: boot nick[@bot]\n"));
    return;
  }
  who = newsplit(&par);
  if (strchr(who, '@') != NULL) {
    char whonick[HANDLEN + 1];

    splitcn(whonick, who, '@', HANDLEN + 1);
    if (!egg_strcasecmp(who, conf.bot->nick)) {
      cmd_boot(u, idx, whonick);
      return;
    }
    if (remote_boots > 0) {
      i = nextbot(who);
      if (i < 0) {
        dprintf(idx, STR("No such bot connected.\n"));
        return;
      }
      botnet_send_reject(i, dcc[idx].nick, conf.bot->nick, whonick,
			 who, par[0] ? par : dcc[idx].nick);
      putlog(LOG_BOTS, "*", STR("#%s# boot %s@%s (%s)"), dcc[idx].nick, whonick,
	     who, par[0] ? par : dcc[idx].nick);
    } else
      dprintf(idx, STR("Remote boots are disabled here.\n"));
    return;
  }
  for (i = 0; i < dcc_total; i++)
    if (!egg_strcasecmp(dcc[i].nick, who)
        && !ok && (dcc[i].type->flags & DCT_CANBOOT)) {
      u2 = get_user_by_handle(userlist, dcc[i].nick);
      if (u2 && (u2->flags & USER_OWNER)
          && egg_strcasecmp(dcc[idx].nick, who)) {
        dprintf(idx, STR("You can't boot a bot owner.\n"));
        return;
      }
      if (u2 && (u2->flags & USER_MASTER) && !(u && (u->flags & USER_MASTER))) {
        dprintf(idx, STR("You can't boot a bot master.\n"));
        return;
      }
      dprintf(idx, STR("Booted %s from the party line.\n"), dcc[i].nick);
      putlog(LOG_CMDS, "*", STR("#%s# boot %s %s"), dcc[idx].nick, who, par);
      do_boot(i, dcc[idx].nick, par);
      ok = 1;
    }
  if (!ok)
    dprintf(idx, STR("Who?  No such person on the party line.\n"));
}

static void cmd_console(struct userrec *u, int idx, char *par)
{
  char *nick = NULL, s[2] = "", s1[512] = "";
  int dest = 0, i, ok = 0, pls, md;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  struct chanset_t *chan = NULL;

  if (!par[0]) {
    dprintf(idx, STR("Your console is %s: %s (%s).\n"),
	    dcc[idx].u.chat->con_chan,
	    masktype(dcc[idx].u.chat->con_flags),
	    maskname(dcc[idx].u.chat->con_flags));
    return;
  }
  get_user_flagrec(u, &fr, dcc[idx].u.chat->con_chan);
  strcpy(s1, par);
  nick = newsplit(&par);
  /* Don't remove '+' as someone couldn't have '+' in CHANMETA cause
   * he doesn't use IRCnet ++rtc.
   */
  if (nick[0] && !strchr(CHANMETA "+-*", nick[0]) && glob_master(fr)) {
    for (i = 0; i < dcc_total; i++)
      if (!egg_strcasecmp(nick, dcc[i].nick) &&
	  (dcc[i].type == &DCC_CHAT) && (!ok)) {
	ok = 1;
	dest = i;
      }
    if (!ok) {
      dprintf(idx, STR("No such user on the party line!\n"));
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
    if (strcmp(nick, "*") && !chan) {
      dprintf(idx, STR("Invalid console channel: %s.\n"), nick);
      return;
    }

    get_user_flagrec(u, &fr, nick);

    if (strcmp(nick, "*") && private(fr, findchan_by_dname(nick), PRIV_OP)) {
      dprintf(idx, STR("Invalid console channel: %s.\n"), nick);
      return;
    }

    if (!chk_op(fr, findchan_by_dname(nick))) {
      dprintf(idx, STR("You don't have op or master access to channel %s.\n"), nick);
      return;
    }
    strncpyz(dcc[dest].u.chat->con_chan, nick, 81);
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
	    md &= ~(LOG_MISC | LOG_CMDS | LOG_FILES | LOG_WALL |
		    LOG_DEBUG);
	}
	if (!glob_owner(fr) && pls)
	  md &= ~(LOG_RAW | LOG_SRVOUT | LOG_BOTNET | LOG_BOTSHARE | LOG_ERRORS | LOG_GETIN | LOG_WARN);
	if (!glob_master(fr) && pls)
	  md &= ~LOG_BOTS;
	if (pls)
	  dcc[dest].u.chat->con_flags |= md;
	else
	  dcc[dest].u.chat->con_flags &= ~md;
      }
    }
  }
  putlog(LOG_CMDS, "*", STR("#%s# console %s"), dcc[idx].nick, s1);
  if (dest == idx) {
    dprintf(idx, STR("Set your console to %s: %s (%s).\n"),
	    dcc[idx].u.chat->con_chan,
	    masktype(dcc[idx].u.chat->con_flags),
	    maskname(dcc[idx].u.chat->con_flags));
  } else {
    dprintf(idx, STR("Set console of %s to %s: %s (%s).\n"), dcc[dest].nick,
	    dcc[dest].u.chat->con_chan,
	    masktype(dcc[dest].u.chat->con_flags),
	    maskname(dcc[dest].u.chat->con_flags));
    dprintf(dest, STR("%s set your console to %s: %s (%s).\n"), dcc[idx].nick,
	    dcc[dest].u.chat->con_chan,
	    masktype(dcc[dest].u.chat->con_flags),
	    maskname(dcc[dest].u.chat->con_flags));
  }
  console_dostore(dest);
}
static void cmd_date(struct userrec *u, int idx, char *par)
{
  char date[50] = "", utctime[50] = "", ltime[50] = "";

  putlog(LOG_CMDS, "*", "#%s# date", dcc[idx].nick);
  egg_strftime(date, sizeof date, "%c %Z", localtime(&now));
#ifndef S_UTCTIME
  sprintf(ltime, "<-- This time is used on the bot.");
#endif /* !S_UTCTIME */
  dprintf(idx, "%s %s\n", date, (ltime && ltime[0]) ? ltime : "");
  egg_strftime(date, sizeof date, "%c %Z", gmtime(&now));
#ifdef S_UTCTIME
  sprintf(utctime, "<-- This time is used on the bot.");
#endif /* S_UTCTIME */
  dprintf(idx, "%s %s\n", date, (utctime && utctime[0]) ? utctime : "");
}

#ifdef HUB
static void cmd_chhandle(struct userrec *u, int idx, char *par)
{
  char hand[HANDLEN + 1] = "", newhand[HANDLEN + 1] = "";
  int i, atr = u ? u->flags : 0, atr2;
  struct userrec *u2 = NULL;

  strncpyz(hand, newsplit(&par), sizeof hand);
  strncpyz(newhand, newsplit(&par), sizeof newhand);

  if (!hand[0] || !newhand[0]) {
    dprintf(idx, STR("Usage: chhandle <oldhandle> <newhandle>\n"));
    return;
  }
  for (i = 0; i < strlen(newhand); i++)
    if ((newhand[i] <= 32) || (newhand[i] >= 127) || (newhand[i] == '@'))
      newhand[i] = '?';
  if (strchr(BADHANDCHARS, newhand[0]) != NULL)
    dprintf(idx, STR("Bizarre quantum forces prevent nicknames from starting with '%c'.\n"),
           newhand[0]);
  else if (get_user_by_handle(userlist, newhand) &&
          egg_strcasecmp(hand, newhand))
    dprintf(idx, STR("Somebody is already using %s.\n"), newhand);
  else {
    u2 = get_user_by_handle(userlist, hand);
    atr2 = u2 ? u2->flags : 0;
    if (!(atr & USER_MASTER) && !(atr2 & USER_BOT))
      dprintf(idx, STR("You can't change handles for non-bots.\n"));
    else if ((bot_flags(u2) & BOT_SHARE) && !(atr & USER_OWNER))
      dprintf(idx, STR("You can't change share bot's nick.\n"));
    else if ((atr2 & USER_OWNER) && !(atr & USER_OWNER) &&
            egg_strcasecmp(dcc[idx].nick, hand))
      dprintf(idx, STR("You can't change a bot owner's handle.\n"));
    else if (isowner(hand) && egg_strcasecmp(dcc[idx].nick, hand))
      dprintf(idx, STR("You can't change a permanent bot owner's handle.\n"));
    else if (!egg_strcasecmp(newhand, conf.bot->nick) && (!(atr2 & USER_BOT) ||
             nextbot(hand) != -1))
      dprintf(idx, STR("Hey! That's MY name!\n"));
    else if (change_handle(u2, newhand)) {
      putlog(LOG_CMDS, "*", STR("#%s# chhandle %s %s"), dcc[idx].nick,
            hand, newhand);
      dprintf(idx, STR("Changed.\n"));
    } else
      dprintf(idx, STR("Failed.\n"));
#ifdef HUB
    write_userfile(idx);
#endif /* HUB */
  }
}
#endif /* HUB */

static void cmd_handle(struct userrec *u, int idx, char *par)
{
  char oldhandle[HANDLEN + 1] = "", newhandle[HANDLEN + 1] = "";
  int i;

  strncpy(newhandle, newsplit(&par), sizeof newhandle);

  if (!newhandle[0]) {
    dprintf(idx, STR("Usage: handle <new-handle>\n"));
    return;
  }
  for (i = 0; i < strlen(newhandle); i++)
    if ((newhandle[i] <= 32) || (newhandle[i] >= 127) || (newhandle[i] == '@'))
      newhandle[i] = '?';
  if (strchr(BADHANDCHARS, newhandle[0]) != NULL) {
    dprintf(idx, STR("Bizarre quantum forces prevent handle from starting with '%c'.\n"),
	    newhandle[0]);
  } else if (get_user_by_handle(userlist, newhandle) &&
	     egg_strcasecmp(dcc[idx].nick, newhandle)) {
    dprintf(idx, STR("Somebody is already using %s.\n"), newhandle);
  } else if (!egg_strcasecmp(newhandle, conf.bot->nick)) {
    dprintf(idx, STR("Hey!  That's MY name!\n"));
  } else {
    strncpyz(oldhandle, dcc[idx].nick, sizeof oldhandle);
    if (change_handle(u, newhandle)) {
      putlog(LOG_CMDS, "*", STR("#%s# handle %s"), oldhandle, newhandle);
      dprintf(idx, STR("Okay, changed.\n"));
    } else
      dprintf(idx, STR("Failed.\n"));
#ifdef HUB
    write_userfile(idx);
#endif /* HUB */
  }
}

#ifdef HUB
static void cmd_chpass(struct userrec *u, int idx, char *par)
{
  char *handle = NULL, *new = NULL, pass[16] = "";
  int atr = u ? u->flags : 0, l;

  if (!par[0])
    dprintf(idx, STR("Usage: chpass <handle> [password]\n"));
  else {
    handle = newsplit(&par);
    u = get_user_by_handle(userlist, handle);
    if (!u)
      dprintf(idx, STR("No such user.\n"));
    else if (!(atr & USER_MASTER) && !(u->flags & USER_BOT))
      dprintf(idx, STR("You can't change passwords for non-bots.\n"));
    else if ((bot_flags(u) & BOT_SHARE) && !(atr & USER_OWNER))
      dprintf(idx, STR("You can't change a share bot's password.\n"));
    else if ((u->flags & USER_OWNER) && !(atr & USER_OWNER) &&
	     egg_strcasecmp(handle, dcc[idx].nick))
      dprintf(idx, STR("You can't change a bot owner's password.\n"));
    else if (isowner(handle) && egg_strcasecmp(dcc[idx].nick, handle))
      dprintf(idx, STR("You can't change a permanent bot owner's password.\n"));
    else if (!par[0]) {
      putlog(LOG_CMDS, "*", STR("#%s# chpass %s [nothing]"), dcc[idx].nick,
	     handle);
      set_user(&USERENTRY_PASS, u, NULL);
      dprintf(idx, STR("Removed password.\n"));
    } else {
      int good = 0;
      l = strlen(new = newsplit(&par));
      if (l > 15)
	new[15] = 0;
      if (!strcmp(new, "rand")) {
        make_rand_str(pass, 15);
        good = 1;
      } else {
        if (goodpass(new, idx, NULL)) {
          sprintf(pass, "%s", new);
          good = 1;
        }
      }
      if (strlen(pass) > 15)
        pass[15] = 0;

      if (good) {
        set_user(&USERENTRY_PASS, u, pass);
        putlog(LOG_CMDS, "*", STR("#%s# chpass %s [something]"), dcc[idx].nick, handle);
        dprintf(idx, STR("Password for '%s' changed to: %s\n"), handle, pass);
#ifdef HUB
        write_userfile(idx);
#endif /* HUB */
      }
    }
  }
}

static void cmd_chsecpass(struct userrec *u, int idx, char *par)
{
  char *handle = NULL, *new = NULL, pass[16] = "";
  int atr = u ? u->flags : 0, l;

  if (!par[0])
    dprintf(idx, STR("Usage: chsecpass <handle> [secpass/rand]\n"));
  else {
    handle = newsplit(&par);
    u = get_user_by_handle(userlist, handle);
    if (!u)
      dprintf(idx, STR("No such user.\n"));
    else if (!(atr & USER_MASTER) && !(u->flags & USER_BOT))
      dprintf(idx, STR("You can't change passwords for non-bots.\n"));
    else if ((bot_flags(u) & BOT_SHARE) && !(atr & USER_OWNER))
      dprintf(idx, STR("You can't change a share bot's password.\n"));
    else if ((u->flags & USER_OWNER) && !(atr & USER_OWNER) &&
	     egg_strcasecmp(handle, dcc[idx].nick))
      dprintf(idx, STR("You can't change a bot owner's secpass.\n"));
    else if (isowner(handle) && egg_strcasecmp(dcc[idx].nick, handle))
      dprintf(idx, STR("You can't change a permanent bot owner's secpass.\n"));
    else if (!par[0]) {
      putlog(LOG_CMDS, "*", STR("#%s# chsecpass %s [nothing]"), dcc[idx].nick,
	     handle);
      set_user(&USERENTRY_SECPASS, u, NULL);
      dprintf(idx, STR("Removed secpass.\n"));
    } else {

      l = strlen(new = newsplit(&par));
      if (l > 15)
	new[15] = 0;
      if (!strcmp(new, "rand")) {
        make_rand_str(pass, 15);
      } else {
        if (strlen(new) < 6) {
          dprintf(idx, STR("Please use at least 6 characters.\n"));
          return;
        } else {
          sprintf(pass, "%s", new);
        }
      }
      if (strlen(pass) > 15)
        pass[15] = 0;
      set_user(&USERENTRY_SECPASS, u, pass);
      putlog(LOG_CMDS, "*", STR("#%s# chsecpass %s [something]"), dcc[idx].nick, handle);
      dprintf(idx, STR("Secpass for '%s' changed to: %s\n"), handle, pass);
#ifdef HUB
      write_userfile(idx);
#endif /* HUB */
    }
  }
}

static void cmd_botcmd(struct userrec *u, int idx, char *par)
{
  tand_t *tbot = NULL;
  int cnt = 0, rleaf = (-1), tbots = 0;
  char *botm = NULL, *cmd = NULL;
  
  botm = newsplit(&par);

  if (par[0])
    cmd = newsplit(&par);

  if (!botm[0] || !cmd || (cmd && !cmd[0])) {
    dprintf(idx, STR("Usage: botcmd <bot> <cmd> [params]\n"));
    return;
  }

  /* the rest of the cmd will be logged remotely */
  putlog(LOG_CMDS, "*", STR("#%s# botcmd %s %s ..."), dcc[idx].nick, botm, cmd);	
  if (!strcmp(botm, "*") && (!strcmp(botm, "di") || !strcmp(botm, "die"))) {
    dprintf(idx, STR("Not a good idea.\n"));
    return;
  }

  if (!strcmp(botm, "?")) {
    for (tbot = tandbot; tbot; tbot = tbot->next) {
      if (bot_hublevel(get_user_by_handle(userlist, tbot->bot)) == 999)
        tbots++;
    }
    if (tbots)
      rleaf = randint(tbots);
  }
  
  for (tbot = tandbot; tbot; tbot = tbot->next) {
    if (!strcmp(botm, "?") && bot_hublevel(get_user_by_handle(userlist, tbot->bot)) != 999)
      continue;

    if ((rleaf != (-1) && cnt == rleaf) || ((rleaf == (-1) && wild_match(botm, tbot->bot)))) {
      send_remote_simul(idx, tbot->bot, cmd, par ? par : "");
    }
    cnt++;
  }

  if (!cnt) {
    dprintf(idx, STR("No bot matching '%s' linked\n"), botm);
    return;
  }
}

static void cmd_hublevel(struct userrec *u, int idx, char *par)
{
  char *handle = NULL, *level = NULL;
  struct bot_addr *bi = NULL, *obi = NULL;
  struct userrec *u1 = NULL;

  putlog(LOG_CMDS, "*", STR("#%s# hublevel %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: hublevel <bot> <level>\n"));
    return;
  }
  handle = newsplit(&par);
  level = newsplit(&par);
  u1 = get_user_by_handle(userlist, handle);
  if (!u1 || !(u1->flags & USER_BOT)) {
    dprintf(idx, STR("Useful only for bots.\n"));
    return;
  }
  dprintf(idx, STR("Changed bot's hublevel.\n"));
  obi = get_user(&USERENTRY_BOTADDR, u1);
  bi = calloc(1, sizeof(struct bot_addr));

  bi->uplink = strdup(obi->uplink);
  bi->address = strdup(obi->address);
  bi->telnet_port = obi->telnet_port;
  bi->relay_port = obi->relay_port;
  bi->hublevel = atoi(level);
  set_user(&USERENTRY_BOTADDR, u1, bi);
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
}

static void cmd_uplink(struct userrec *u, int idx, char *par)
{
  char *handle = NULL, *uplink = NULL;
  struct bot_addr *bi = NULL, *obi = NULL;
  struct userrec *u1 = NULL;

  putlog(LOG_CMDS, "*", STR("#%s# uplink %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: uplink <bot> [uplink]\n"));
    return;
  }
  handle = newsplit(&par);
  uplink = newsplit(&par);
  if (!uplink)
    uplink = "";
  u1 = get_user_by_handle(userlist, handle);
  if (!u1 || !(u1->flags & USER_BOT)) {
    dprintf(idx, STR("Useful only for bots.\n"));
    return;
  }
  if (uplink && uplink[0])
    dprintf(idx, STR("Changed bot's uplink.\n"));
  else
    dprintf(idx, STR("Cleared bot's uplink.\n"));
  obi = get_user(&USERENTRY_BOTADDR, u1);
  bi = calloc(1, sizeof(struct bot_addr));

  bi->uplink = strdup(uplink);
  bi->address = strdup(obi->address);
  bi->telnet_port = obi->telnet_port;
  bi->relay_port = obi->relay_port;
  bi->hublevel = obi->hublevel;
  set_user(&USERENTRY_BOTADDR, u1, bi);
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
}


static void cmd_chaddr(struct userrec *u, int idx, char *par)
{
  int telnet_port = 3333, relay_port = 3333;
#ifdef USE_IPV6
  char *handle = NULL, *addr = NULL, *p = NULL, *q = NULL, *r = NULL;
#else
  char *handle = NULL, *addr = NULL, *p = NULL, *q = NULL;
#endif /* USE_IPV6 */
  struct bot_addr *bi = NULL, *obi = NULL;
  struct userrec *u1 = NULL;

  handle = newsplit(&par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: chaddr <bot> <address[:telnet-port[/relay-port]]>\n"));
    return;
  }
  addr = newsplit(&par);
  if (strlen(addr) > UHOSTMAX)
    addr[UHOSTMAX] = 0;
  u1 = get_user_by_handle(userlist, handle);
  if (!u1 || !(u1->flags & USER_BOT)) {
    dprintf(idx, STR("This command is only useful for tandem bots.\n"));
    return;
  }
  if ((bot_flags(u1) & BOT_SHARE) && (!u || !u->flags & USER_OWNER)) {
    dprintf(idx, STR("You can't change a share bot's address.\n"));
    return;
  }
  putlog(LOG_CMDS, "*", STR("#%s# chaddr %s %s"), dcc[idx].nick, handle, addr);
  dprintf(idx, STR("Changed bot's address.\n"));

  obi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, u1);
  bi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, u1);
  if (bi) {
    telnet_port = bi->telnet_port;
    relay_port = bi->relay_port;
  }

  bi = calloc(1, sizeof(struct bot_addr));

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

      bi->address = calloc(1, r - addr + 1);	/* alloc and copy the addr */
      strncpyz(bi->address, addr, r - addr + 1);

//      addr = r;					/* move up addr to the ']' */
//      addr++;					/* move up addr tot he ':' */
      q = r + 1;				/* set q to ':' at addr */
    } else {
#endif /* !USE_IPV6 */
      bi->address = calloc(1, q - addr + 1);
      strncpyz(bi->address, addr, q - addr + 1);
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
#endif /* HUB */

static void cmd_comment(struct userrec *u, int idx, char *par)
{
  char *handle = NULL;
  struct userrec *u1 = NULL;

  handle = newsplit(&par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: comment <handle> <newcomment/none>\n"));
    return;
  }
  u1 = get_user_by_handle(userlist, handle);
  if (!u1) {
    dprintf(idx, STR("No such user!\n"));
    return;
  }
  if ((u1->flags & USER_OWNER) && !(u && (u->flags & USER_OWNER)) &&
      egg_strcasecmp(handle, dcc[idx].nick)) {
    dprintf(idx, STR("You can't change comment on a bot owner.\n"));
    return;
  }
  putlog(LOG_CMDS, "*", STR("#%s# comment %s %s"), dcc[idx].nick, handle, par);
  if (!egg_strcasecmp(par, "none")) {
    dprintf(idx, STR("Okay, comment blanked.\n"));
    set_user(&USERENTRY_COMMENT, u1, NULL);
    return;
  }
  dprintf(idx, STR("Changed comment.\n"));
  update_mod(handle, dcc[idx].nick, "comment", NULL);
  set_user(&USERENTRY_COMMENT, u1, par);
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
}

static void cmd_randstring(struct userrec *u, int idx, char *par)
{
  int len;

  if (!par[0]) {
    dprintf(idx, "Usage: randstring <len>\n");
    return;
  }

  putlog(LOG_CMDS, "*", STR("#%s# randstring %s"), dcc[idx].nick, par);

  len = atoi(par);
  if (len < 301) {
    char *rand = NULL;

    rand = malloc(len + 1);
    make_rand_str(rand, len);
    dprintf(idx, STR("string: %s\n"), rand);
    free(rand);
  } else 
    dprintf(idx, "Too long, must be <= 300\n");
}

static void cmd_restart(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# restart"), dcc[idx].nick);
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
#ifdef LEAF
  nuke_server("Restarting...");
#endif /* LEAF */
  botnet_send_chat(-1, conf.bot->nick, "Restarting...");
  botnet_send_bye();

  fatal("Restarting...", 1);
  usleep(2000 * 500);
  unlink(conf.bot->pid_file); //if this fails it is ok, cron will restart the bot, *hopefully*
  system(binname); //start new bot.
  exit(0);
}

#ifdef HUB
static void cmd_reload(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# reload"), dcc[idx].nick);
  dprintf(idx, STR("Reloading user file...\n"));
  reload();
}
#endif /* HUB */

static void cmd_die(struct userrec *u, int idx, char *par)
{
  char s1[1024] = "", s2[1024] = "";

  putlog(LOG_CMDS, "*", STR("#%s# die %s"), dcc[idx].nick, par);
  if (par[0]) {
    egg_snprintf(s1, sizeof s1, STR("BOT SHUTDOWN (%s: %s)"), dcc[idx].nick, par);
    egg_snprintf(s2, sizeof s2, STR("DIE BY %s!%s (%s)"), dcc[idx].nick, dcc[idx].host, par);
    strncpyz(quit_msg, par, 1024);
  } else {
    egg_snprintf(s1, sizeof s1, STR("BOT SHUTDOWN (Authorized by %s)"), dcc[idx].nick);
    egg_snprintf(s2, sizeof s2, STR("DIE BY %s!%s (request)"), dcc[idx].nick, dcc[idx].host);
    strncpyz(quit_msg, dcc[idx].nick, 1024);
  }
  kill_bot(s1, s2);
}

static void cmd_debug(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# debug"), dcc[idx].nick);
  tell_netdebug(idx);
}

static void cmd_timers(struct userrec *u, int idx, char *par)
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
      egg_snprintf(interval, sizeof interval, "(%d.%d secs)", howlong.sec, howlong.usec);
      egg_snprintf(next, sizeof next, "%d.%d secs", diff.sec, diff.usec);
      dprintf(idx, "%-2d: %-25s %-15s Next: %-25s Called: %d\n", i, name, interval, next, called);
    }
    free(ids);
  }
}

static void cmd_simul(struct userrec *u, int idx, char *par)
{
  char *nick = NULL;
  int i, ok = 0;

  nick = newsplit(&par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: simul <hand> <text>\n"));
    return;
  }
  if (isowner(nick)) {
    dprintf(idx, STR("Unable to '.simul' permanent owners.\n"));
    return;
  }
  for (i = 0; i < dcc_total; i++)
    if (!egg_strcasecmp(nick, dcc[i].nick) && !ok &&
	(dcc[i].type->flags & DCT_SIMUL)) {
      putlog(LOG_CMDS, "*", STR("#%s# simul %s %s"), dcc[idx].nick, nick, par);
      if (dcc[i].type && dcc[i].type->activity) {
	dcc[i].type->activity(i, par, strlen(par));
	ok = 1;
      }
    }
  if (!ok)
    dprintf(idx, STR("No such user on the party line.\n"));
}

#ifdef HUB
static void cmd_link(struct userrec *u, int idx, char *par)
{
  char *s = NULL;
  int i;

  if (!par[0]) {
    dprintf(idx, STR("Usage: link [some-bot] <new-bot>\n"));
    return;
  }
  putlog(LOG_CMDS, "*", STR("#%s# link %s"), dcc[idx].nick, par);
  s = newsplit(&par);
  if (!par[0] || !egg_strcasecmp(par, conf.bot->nick))
    botlink(dcc[idx].nick, idx, s);
  else {
    char x[40] = "";

    i = nextbot(s);
    if (i < 0) {
      dprintf(idx, STR("No such bot online.\n"));
      return;
    }
    simple_sprintf(x, "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, conf.bot->nick);
    botnet_send_link(i, x, s, par);
  }
}
#endif /* HUB */

static void cmd_unlink(struct userrec *u, int idx, char *par)
{
  int i;
  char *bot = NULL;

  if (!par[0]) {
    dprintf(idx, STR("Usage: unlink <bot> [reason]\n"));
    return;
  }
  putlog(LOG_CMDS, "*", STR("#%s# unlink %s"), dcc[idx].nick, par);
  bot = newsplit(&par);
  i = nextbot(bot);
  if (i < 0) {
    botunlink(idx, bot, par);
    return;
  }
  /* If we're directly connected to that bot, just do it
   * (is nike gunna sue?)
   */
  if (!egg_strcasecmp(dcc[i].nick, bot))
    botunlink(idx, bot, par);
  else {
    char x[40] = "";

    simple_sprintf(x, "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, conf.bot->nick);
    botnet_send_unlink(i, x, lastbot(bot), bot, par);
  }
}

static void cmd_relay(struct userrec *u, int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, STR("Usage: relay <bot>\n"));
    return;
  }
  putlog(LOG_CMDS, "*", STR("#%s# relay %s"), dcc[idx].nick, par);
  tandem_relay(idx, par, 0);
}

#ifdef HUB
static void cmd_save(struct userrec *u, int idx, char *par)
{
  char buf[100] = "";
  int i = 0;

  putlog(LOG_CMDS, "*", STR("#%s# save"), dcc[idx].nick);
  sprintf(buf, "Saving user file...");
  i = write_userfile(-1);
  if (i == 0)
    strcat(buf, "success.");
  else if (i == 1)
    strcat(buf, "failed: No userlist.");
  else if (i == 2)
    strcat(buf, "failed: Cannot open userfile for writing.");
  else if (i == 3)
    strcat(buf, "failed: Problem writing users/chans (see debug).");
  else		/* This can't happen. */
    strcat(buf, "failed: Unforseen error");

  dprintf(idx, "%s\n", buf);
}

static void cmd_backup(struct userrec *u, int idx, char *par)
{
  
  putlog(LOG_CMDS, "*", STR("#%s# backup"), dcc[idx].nick);
  dprintf(idx, STR("Backing up the channel & user files...\n"));
#ifdef HUB
  write_userfile(idx);
  backup_userfile();
#endif /* HUB */
}

static void cmd_trace(struct userrec *u, int idx, char *par)
{
  int i;
  char x[NOTENAMELEN + 11] = "", y[11] = "";

  if (!par[0]) {
    dprintf(idx, STR("Usage: trace <bot>\n"));
    return;
  }
  if (!egg_strcasecmp(par, conf.bot->nick)) {
    dprintf(idx, STR("That's me!  Hiya! :)\n"));
    return;
  }
  i = nextbot(par);
  if (i < 0) {
    dprintf(idx, STR("Unreachable bot.\n"));
    return;
  }
  putlog(LOG_CMDS, "*", STR("#%s# trace %s"), dcc[idx].nick, par);
  simple_sprintf(x, "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, conf.bot->nick);
  simple_sprintf(y, ":%d", now);
  botnet_send_trace(i, x, par, y);
}
#endif /* HUB */

/* After messing with someone's user flags, make sure the dcc-chat flags
 * are set correctly.
 */
int check_dcc_attrs(struct userrec *u, int oatr)
{
  int i, stat;
 
  if (!u)
    return 0;

  /* Make sure default owners are +a */
  if (isowner(u->handle)) {
    u->flags = sanity_check(u->flags | USER_ADMIN);
  }

  for (i = 0; i < dcc_total; i++) {
    if ((dcc[i].type->flags & DCT_MASTER) &&
	(!egg_strcasecmp(u->handle, dcc[i].nick))) {
      stat = dcc[i].status;
      if ((dcc[i].type == &DCC_CHAT) &&
	  ((u->flags & (USER_OP | USER_MASTER | USER_OWNER))
	   != (oatr & (USER_OP | USER_MASTER | USER_OWNER)))) {
	botnet_send_join_idx(i, -1);
      }
      if ((oatr & USER_MASTER) && !(u->flags & USER_MASTER)) {
	struct flag_record fr = {FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0};

	dcc[i].u.chat->con_flags &= ~(LOG_MISC | LOG_CMDS | LOG_RAW |
				      LOG_FILES | LOG_WALL | LOG_DEBUG);
	get_user_flagrec(u, &fr, NULL);
	if (!chan_master(fr))
	  dcc[i].u.chat->con_flags |= (LOG_MISC | LOG_CMDS);
	dprintf(i, STR("*** POOF! ***\n"));
	dprintf(i, STR("You are no longer a master on this bot.\n"));
      }
      if (!(oatr & USER_MASTER) && (u->flags & USER_MASTER)) {
	dcc[i].u.chat->con_flags |= conmask;
	dprintf(i, STR("*** POOF! ***\n"));
	dprintf(i, STR("You are now a master on this bot.\n"));
      }
      if (!(oatr & USER_PARTY) && (u->flags & USER_PARTY) && dcc[i].u.chat->channel < 0) {
        dprintf(i, "-+- POOF! -+-\n");
        dprintf(i, STR("You now have party line chat access.\n"));
        dprintf(i, STR("To rejoin the partyline, type: %schat on\n"), dcc_prefix);
      }

      if (!(oatr & USER_OWNER) && (u->flags & USER_OWNER)) {
	dprintf(i, STR("@@@ POOF! @@@\n"));
	dprintf(i, STR("You are now an OWNER of this bot.\n"));
      }
      if ((oatr & USER_OWNER) && !(u->flags & USER_OWNER)) {
	dprintf(i, STR("@@@ POOF! @@@\n"));
	dprintf(i, STR("You are no longer an owner of this bot.\n"));
      }

      if (!(u->flags & USER_PARTY) && dcc[i].u.chat->channel >= 0) { //who cares about old flags, they shouldnt be here anyway.
        dprintf(i, STR("-+- POOF! -+-\n"));
        dprintf(i, STR("You no longer have party line chat access.\n"));
        dprintf(i, STR("Leaving chat mode...\n"));
        chanout_but(-1, dcc[i].u.chat->channel, STR("*** %s left the party line - no chat access.\n"), dcc[i].nick);
        if (dcc[i].u.chat->channel < 100000)
          botnet_send_part_idx(i, "");
        dcc[i].u.chat->channel = (-1);
      }

      if (!(oatr & USER_ADMIN) && (u->flags & USER_ADMIN)) {
	dprintf(i, STR("^^^ POOF! ^^^\n"));
	dprintf(i, STR("You are now an ADMIN of this bot.\n"));
      }
      if ((oatr & USER_ADMIN) && !(u->flags & USER_ADMIN)) {
	dprintf(i, STR("^^^ POOF! ^^^\n"));
	dprintf(i, STR("You are no longer an admin of this bot.\n"));
      }
      if ((stat & STAT_PARTY) && (u->flags & USER_OP))
	stat &= ~STAT_PARTY;
      if (!(stat & STAT_PARTY) && !(u->flags & USER_OP) &&
	  !(u->flags & USER_MASTER))
	stat |= STAT_PARTY;
      if (stat & STAT_CHAT) {
#ifdef HUB
       if (!(u->flags & USER_HUBA))        
         stat &= ~STAT_CHAT;
#endif /* HUB */
       if (ischanhub() && !(u->flags & USER_CHUBA))
         stat &= ~STAT_CHAT;
      }
      if ((u->flags & USER_PARTY))
	stat |= STAT_CHAT;
      dcc[i].status = stat;
      /* Check if they no longer have access to wherever they are.
       */
#ifdef HUB
      if (!(u->flags & (USER_HUBA))) {
        /* no hub access, drop them. */
        dprintf(i, STR("-+- POOF! -+-\n"));
        dprintf(i, STR("You no longer have hub access.\n\n"));
        do_boot(i, conf.bot->nick, STR("No hub access.\n\n"));
      }     
#else /* !HUB */
      if (ischanhub() && !(u->flags & (USER_CHUBA))) {
        /* no chanhub access, drop them. */
        dprintf(i, STR("-+- POOF! -+-\n"));
        dprintf(i, STR("You no longer have chathub access.\n\n"));
        do_boot(i, conf.bot->nick, STR("No chathub access.\n\n"));
      }
#endif /* HUB */
    }
    if (dcc[i].type == &DCC_BOT && !egg_strcasecmp(u->handle, dcc[i].nick)) {
      if ((dcc[i].status & STAT_LEAF) && !(u->flags & BOT_LEAF))
	dcc[i].status &= ~(STAT_LEAF | STAT_WARNED);
      if (!(dcc[i].status & STAT_LEAF) && (u->flags & BOT_LEAF))
	dcc[i].status |= STAT_LEAF;
    }
  }
  return u->flags;
}

int check_dcc_chanattrs(struct userrec *u, char *chname, int chflags, int ochatr)
{
  int i, found = 0, atr = u ? u->flags : 0;
  struct chanset_t *chan = NULL;

  if (!u)
    return 0;
  for (i = 0; i < dcc_total; i++) {
    if ((dcc[i].type->flags & DCT_MASTER) &&
	!egg_strcasecmp(u->handle, dcc[i].nick)) {
      if ((dcc[i].type == &DCC_CHAT) &&
	  ((chflags & (USER_OP | USER_MASTER | USER_OWNER))
	   != (ochatr & (USER_OP | USER_MASTER | USER_OWNER))))
	botnet_send_join_idx(i, -1);
      if ((ochatr & USER_MASTER) && !(chflags & USER_MASTER)) {
	if (!(atr & USER_MASTER))
	  dcc[i].u.chat->con_flags &= ~(LOG_MISC | LOG_CMDS);
	dprintf(i, STR("*** POOF! ***\n"));
	dprintf(i, STR("You are no longer a master on %s.\n"), chname);
      }
      if (!(ochatr & USER_MASTER) && (chflags & USER_MASTER)) {
	dcc[i].u.chat->con_flags |= conmask;
	if (!(atr & USER_MASTER))
	  dcc[i].u.chat->con_flags &=
	    ~(LOG_RAW | LOG_DEBUG | LOG_WALL | LOG_FILES | LOG_SRVOUT);
	dprintf(i, STR("*** POOF! ***\n"));
	dprintf(i, STR("You are now a master on %s.\n"), chname);
      }
      if (!(ochatr & USER_OWNER) && (chflags & USER_OWNER)) {
	dprintf(i, STR("@@@ POOF! @@@\n"));
	dprintf(i, STR("You are now an OWNER of %s.\n"), chname);
      }
      if ((ochatr & USER_OWNER) && !(chflags & USER_OWNER)) {
	dprintf(i, STR("@@@ POOF! @@@\n"));
	dprintf(i, STR("You are no longer an owner of %s.\n"), chname);
      }
      if (((ochatr & (USER_OP | USER_MASTER | USER_OWNER)) &&
	   (!(chflags & (USER_OP | USER_MASTER | USER_OWNER)))) ||
	  ((chflags & (USER_OP | USER_MASTER | USER_OWNER)) &&
	   (!(ochatr & (USER_OP | USER_MASTER | USER_OWNER))))) {
	struct flag_record fr = {FR_CHAN, 0, 0, 0, 0, 0};

	for (chan = chanset; chan && !found; chan = chan->next) {
	  get_user_flagrec(u, &fr, chan->dname);
	  if (fr.chan & (USER_OP | USER_MASTER | USER_OWNER))
	    found = 1;
	}
	if (!chan)
	  chan = chanset;
	if (chan)
	  strcpy(dcc[i].u.chat->con_chan, chan->dname);
	else
	  strcpy(dcc[i].u.chat->con_chan, "*");
      }
    }
  }
  return chflags;
}

static void cmd_chattr(struct userrec *u, int idx, char *par)
{
  char *hand = NULL, *arg = NULL, *tmpchg = NULL, *chg = NULL, work[1024] = "";
  struct chanset_t *chan = NULL;
  struct userrec *u2 = NULL;
  struct flag_record pls = {0, 0, 0, 0, 0, 0},
  		     mns = {0, 0, 0, 0, 0, 0},
		     user = {0, 0, 0, 0, 0, 0},
		     ouser = {0, 0, 0, 0, 0, 0};
  int fl = -1, of = 0, ocf = 0;

  if (!par[0]) {
    dprintf(idx, STR("Usage: chattr <handle> [changes] [channel]\n"));
    return;
  }
  hand = newsplit(&par);
  u2 = get_user_by_handle(userlist, hand);
  if (!u2) {
    dprintf(idx, STR("No such user!\n"));
    return;
  }

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
  Assert(!(!arg && chan));
  if (arg && !chan) {
    dprintf(idx, STR("No channel record for %s.\n"), arg);
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
        dprintf (idx, STR("Invalid console channel %s.\n"), arg);
	return;
      }
    } else if (arg && !strpbrk(chg, "&|")) {
      tmpchg = calloc(1, strlen(chg) + 2);
      strcpy(tmpchg, "|");
      strcat(tmpchg, chg);
      chg = tmpchg;
    }
  }
  par = arg;
  user.match = FR_GLOBAL;
  if (chan)
    user.match |= FR_CHAN;
  get_user_flagrec(u, &user, chan ? chan->dname : 0);
  get_user_flagrec(u2, &ouser, chan ? chan->dname : 0);
  if (chan && !glob_master(user) && !chan_master(user)) {
    dprintf(idx, STR("You do not have channel master privileges for channel %s.\n"),
	    par);
    if (tmpchg)
      free(tmpchg);
    return;
  }
    if (chan && private(user, chan, PRIV_OP)) {
      dprintf(idx, STR("You do not have access to change flags for %s\n"), chan->dname);
      if (tmpchg)
        free(tmpchg);
      return;
    }

  user.match &= fl;
  if (chg) {
    pls.match = user.match;
    break_down_flags(chg, &pls, &mns);
    /* No-one can change these flags on-the-fly */
    pls.global &= ~(USER_BOT);
    mns.global &= ~(USER_BOT);

    if ((pls.global & USER_UPDATEHUB) && (bot_hublevel(u2) == 999)) {
      dprintf(idx, STR("Only a hub can be set as the updatehub.\n"));
      pls.global &= ~(USER_UPDATEHUB);
    }
    
    /* strip out +p without +i or +j */
    if (((pls.global & USER_PARTY) && !(pls.global & USER_CHUBA) && !(pls.global & USER_HUBA)) && (!glob_huba(ouser) && !glob_chuba(ouser))) {
      dprintf(idx, "Global flag +p requires either chathub or hub access first.\n");
      pls.global &= ~USER_PARTY;
    }
    
    if (!isowner(u->handle)) {
      if (pls.global & USER_HUBA)
        putlog(LOG_MISC, "*", STR("%s attempted to give %s hub connect access"), dcc[idx].nick, u2->handle);
      if (mns.global & USER_HUBA)
        putlog(LOG_MISC, "*", STR("%s attempted to take away hub connect access from %s"), dcc[idx].nick, u2->handle);
      if (pls.global & USER_UPDATEHUB)
        putlog(LOG_MISC, "*", STR("%s attempted to make %s the updatehub"), dcc[idx].nick, u2->handle);
      if (mns.global & USER_UPDATEHUB)
        putlog(LOG_MISC, "*", STR("%s attempted to take away updatehub status from %s"), dcc[idx].nick, u2->handle);
      if (pls.global & USER_ADMIN)
        putlog(LOG_MISC, "*", STR("%s attempted to give %s admin access"), dcc[idx].nick, u2->handle);
      if (mns.global & USER_ADMIN)
        putlog(LOG_MISC, "*", STR("%s attempted to take away admin access from %s"), dcc[idx].nick, u2->handle);
      if (pls.global & USER_OWNER)
        putlog(LOG_MISC, "*", STR("%s attempted to give owner to %s"), dcc[idx].nick, u2->handle);
      if (mns.global & USER_OWNER)
        putlog(LOG_MISC, "*", STR("%s attempted to take owner away from %s"), dcc[idx].nick, u2->handle);
      pls.global &=~(USER_HUBA | USER_ADMIN | USER_OWNER | USER_UPDATEHUB);
      mns.global &=~(USER_HUBA | USER_ADMIN | USER_OWNER | USER_UPDATEHUB);
      pls.chan &= ~(USER_ADMIN | USER_UPDATEHUB);
    }
    if (chan) {
      pls.chan &= ~(BOT_SHARE | USER_HUBA | USER_CHUBA);
      mns.chan &= ~(BOT_SHARE | USER_HUBA | USER_CHUBA);
    }
    if (!glob_owner(user) && !isowner(u->handle)) {
      pls.global &= ~(USER_CHUBA | USER_OWNER | USER_MASTER | USER_UNSHARED);
      mns.global &= ~(USER_CHUBA | USER_OWNER | USER_MASTER | USER_UNSHARED);

      if (chan) {
	pls.chan &= ~USER_OWNER;
	mns.chan &= ~USER_OWNER;
      }
    }
    if (chan && !chan_owner(user) && !glob_owner(user) && !isowner(u->handle)) {
      pls.chan &= ~USER_MASTER;
      mns.chan &= ~USER_MASTER;
      if (!chan_master(user) && !glob_master(user)) {
	pls.chan = 0;
	mns.chan = 0;
      }
    }
#ifdef LEAF
    pls.global &=~(USER_OWNER | USER_ADMIN | USER_HUBA | USER_CHUBA);
    mns.global &=~(USER_OWNER | USER_ADMIN | USER_HUBA | USER_CHUBA);
#endif /* LEAF */
    get_user_flagrec(u2, &user, par);
    if (user.match & FR_GLOBAL) {
      of = user.global;
      user.global = sanity_check((user.global |pls.global) &~mns.global);

      user.udef_global = (user.udef_global | pls.udef_global)
	& ~mns.udef_global;
    }
    if (chan) {
      ocf = user.chan;
      user.chan = chan_sanity_check((user.chan | pls.chan) & ~mns.chan,
				    user.global);

      user.udef_chan = (user.udef_chan | pls.udef_chan) & ~mns.udef_chan;
    }
    set_user_flagrec(u2, &user, par);
  }
  if (chan) {
    char tmp[100] = "";

    putlog(LOG_CMDS, "*", STR("#%s# (%s) chattr %s %s"), dcc[idx].nick, chan ? chan->dname : "*", hand, chg ? chg : "");
    egg_snprintf(tmp, sizeof tmp, "chattr %s", chg);
    update_mod(hand, dcc[idx].nick, tmp, chan->dname);
  } else {
    putlog(LOG_CMDS, "*", STR("#%s# chattr %s %s"), dcc[idx].nick, hand, chg ? chg : "");
    update_mod(hand, dcc[idx].nick, "chattr", chg);
  }
  /* Get current flags and display them */
  if (user.match & FR_GLOBAL) {
    user.match = FR_GLOBAL;
    if (chg)
      check_dcc_attrs(u2, of);
    get_user_flagrec(u2, &user, NULL);
    build_flags(work, &user, NULL);
    if (work[0] != '-')
      dprintf(idx, STR("Global flags for %s are now +%s.\n"), hand, work);
    else
      dprintf(idx, STR("No global flags for %s.\n"), hand);
  }
  if (chan) {
    user.match = FR_CHAN;
    get_user_flagrec(u2, &user, par);
    user.chan &= ~BOT_SHARE;
    if (chg)
      check_dcc_chanattrs(u2, chan->dname, user.chan, ocf);
    build_flags(work, &user, NULL);
    if (work[0] != '-')
      dprintf(idx, STR("Channel flags for %s on %s are now +%s.\n"), hand,
	      chan->dname, work);
    else
      dprintf(idx, STR("No flags for %s on %s.\n"), hand, chan->dname);
  }
#ifdef LEAF
  if (chg)
    check_this_user(hand, 0, NULL);
#endif /* LEAF */
  if (tmpchg)
    free(tmpchg);
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
}

static void cmd_chat(struct userrec *u, int idx, char *par)
{
  char *arg = NULL;
  int newchan, oldchan;

  if (!(u->flags & USER_PARTY)) {
    dprintf(idx, STR("You don't have partyline access\n"));
    return;
  }

  arg = newsplit(&par);
  if (!egg_strcasecmp(arg, "off")) {
    /* Turn chat off */
    if (dcc[idx].u.chat->channel < 0) {
      dprintf(idx, STR("You weren't in chat anyway!\n"));
      return;
    } else {
      dprintf(idx, STR("Leaving chat mode...\n"));
      check_bind_chpt(conf.bot->nick, dcc[idx].nick, dcc[idx].sock,
		     dcc[idx].u.chat->channel);
      chanout_but(-1, dcc[idx].u.chat->channel,
		  "*** %s left the party line.\n",
		  dcc[idx].nick);
      if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
	botnet_send_part_idx(idx, "");
    }
    dcc[idx].u.chat->channel = (-1);
  } else {
    if (arg[0] == '*') {
      if (((arg[1] < '0') || (arg[1] > '9'))) {
	if (!arg[1])
	  newchan = 0;
	else {
          newchan = -1;
	}
	if (newchan < 0) {
	  dprintf(idx, STR("No channel exists by that name.\n"));
	  return;
	}
      } else
	newchan = GLOBAL_CHANS + atoi(arg + 1);
      if (newchan < GLOBAL_CHANS || newchan > 199999) {
	dprintf(idx, STR("Channel number out of range: local channels must be *0-*99999.\n"));
	return;
      }
    } else {
      if (((arg[0] < '0') || (arg[0] > '9')) && (arg[0])) {
	if (!egg_strcasecmp(arg, "on"))
	  newchan = 0;
	else {
          newchan = -1;
	}
	if (newchan < 0) {
	  dprintf(idx, STR("No channel exists by that name.\n"));
	  return;
	}
      } else
	newchan = atoi(arg);
      if ((newchan < 0) || (newchan > 99999)) {
	dprintf(idx, STR("Channel number out of range: must be between 0 and 99999.\n"));
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
	dprintf(idx, STR("You're already on the party line!\n"));
        return;
      } else {
	dprintf(idx, STR("You're already on channel %s%d!\n"),
		(newchan < GLOBAL_CHANS) ? "" : "*", newchan % GLOBAL_CHANS);
        return;
      }
    } else {
      oldchan = dcc[idx].u.chat->channel;
      if (oldchan >= 0)
	check_bind_chpt(conf.bot->nick, dcc[idx].nick, dcc[idx].sock, oldchan);
      if (!oldchan) {
	chanout_but(-1, 0, "*** %s left the party line.\n", dcc[idx].nick);
      } else if (oldchan > 0) {
	chanout_but(-1, oldchan, "*** %s left the channel.\n", dcc[idx].nick);
      }
      dcc[idx].u.chat->channel = newchan;
      if (!newchan) {
	dprintf(idx, STR("Entering the party line...\n"));
	chanout_but(-1, 0, "*** %s joined the party line.\n", dcc[idx].nick);
      } else {
	dprintf(idx, STR("Joining channel '%s'...\n"), arg);
	chanout_but(-1, newchan, "*** %s joined the channel.\n", dcc[idx].nick);
      }
      check_bind_chjn(conf.bot->nick, dcc[idx].nick, newchan, geticon(idx),
		     dcc[idx].sock, dcc[idx].host);
      if (newchan < GLOBAL_CHANS)
	botnet_send_join_idx(idx, oldchan);
      else if (oldchan < GLOBAL_CHANS)
	botnet_send_part_idx(idx, "");
    }
  }
  console_dostore(idx);
}

int exec_str(int idx, char *cmd) {
  char *out = NULL, *err = NULL, *p = NULL, *np = NULL;

  if (shell_exec(cmd, NULL, &out, &err)) {
    if (out) {
      dprintf(idx, "Result:\n");
      p=out;
      while (p && p[0]) {
        np=strchr(p, '\n');
        if (np)
          *np++=0;
        dprintf(idx, "%s\n", p);
        p=np;
      }
      dprintf(idx, "\n");
      free(out);
    }
    if (err) {
      dprintf(idx, "Errors:\n");
      p=err;
      while (p && p[0]) {
        np=strchr(p, '\n');
        if (np)
          *np++=0;
        dprintf(idx, "%s\n", p);
        p=np;
      }
      dprintf(idx, "\n");
      free(err);
    }
    return 1;
  }
  return 0;
}

static void cmd_exec(struct userrec *u, int idx, char *par) {
  putlog(LOG_CMDS, "*", "#%s# exec %s", dcc[idx].nick, par);
#ifdef LEAF
  if (!isowner(u->handle)) {
    putlog(LOG_WARN, "*", STR("%s attempted 'exec' %s"), dcc[idx].nick, par);
    dprintf(idx, STR("exec is only available to permanent owners on leaf bots\n"));
    return;
  }
#endif /* LEAF */
  if (exec_str(idx, par))
    dprintf(idx, "Exec completed\n");
  else
    dprintf(idx, "Exec failed\n");
}

static void cmd_w(struct userrec *u, int idx, char *par) {
  putlog(LOG_CMDS, "*", "#%s# w", dcc[idx].nick);
  if (!exec_str(idx, "w"))
    dprintf(idx, "Exec failed\n");
}

static void cmd_ps(struct userrec *u, int idx, char *par) {
  char *buf = NULL;

  putlog(LOG_CMDS, "*", STR("#%s# ps %s"), dcc[idx].nick, par);
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    putlog(LOG_WARN, "*", STR("%s attempted 'ps' with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, STR("No."));
    return;
  }
  buf = malloc(strlen(par) + 10);
  sprintf(buf, STR("ps %s"), par);
  if (!exec_str(idx, buf))
    dprintf(idx, STR("Exec failed\n"));
  free(buf);
}

static void cmd_last(struct userrec *u, int idx, char *par) {
  char user[20] = "", buf[30] = "";

  putlog(LOG_CMDS, "*", STR("#%s# last %s"), dcc[idx].nick, par);
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    putlog(LOG_WARN, "*", STR("%s attempted 'last' with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, STR("No."));
    return;
  }
  if (par && par[0]) {
    strncpyz(user, par, sizeof(user));
  } else {
    strncpyz(user, conf.username, sizeof(user));
  }
  if (!user[0]) {
    dprintf(idx, STR("Can't determine user id for process\n"));
    return;
  }
  sprintf(buf, STR("last %s"), user);
  if (!exec_str(idx, buf))
    dprintf(idx, STR("Failed to execute /bin/sh last\n"));
}

static void cmd_echo(struct userrec *u, int idx, char *par)
{
  if (!par[0]) {
    dprintf(idx, STR("Echo is currently %s.\n"), dcc[idx].status & STAT_ECHO ? "on" : "off");
    return;
  }
  if (!egg_strcasecmp(par, "on")) {
    dprintf(idx, STR("Echo turned on.\n"));
    dcc[idx].status |= STAT_ECHO;
  } else if (!egg_strcasecmp(par, "off")) {
    dprintf(idx, STR("Echo turned off.\n"));
    dcc[idx].status &= ~STAT_ECHO;
  } else {
    dprintf(idx, STR("Usage: echo <on/off>\n"));
    return;
  }
  console_dostore(idx);
}

static void cmd_color(struct userrec *u, int idx, char *par)
{
  int ansi = 0;
  char *of = NULL;

  putlog(LOG_CMDS, "*", STR("#%s# color %s"), dcc[idx].nick, par);

  if ((idx && ((dcc[idx].type != &DCC_RELAYING) && (dcc[idx].status & STAT_TELNET))) || !backgrd)
    ansi++;
  if (!par[0]) {
    dprintf(idx, STR("Usage: color <on/off>\n"));
    if (dcc[idx].status & STAT_COLOR) 
      dprintf(idx, STR("Color is currently on. (%s)\n"), ansi ? "ANSI" : "mIRC");
    else
      dprintf(idx, STR("Color is currently off.\n"));
    return;
  }
  of = newsplit(&par);

  if (!egg_strcasecmp(of, "on")) {
    dcc[idx].status |= STAT_COLOR;
  } else if (!egg_strcasecmp(of, "off")) {
    dcc[idx].status &= ~(STAT_COLOR);
    dprintf(idx, STR("Color turned off.\n"));
  } else {
    return;
  }
  console_dostore(idx);
}

int stripmodes(char *s)
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

char *stripmasktype(int x)
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

static char *stripmaskname(int x)
{
  static char s[161] = "";
  int i = 0;

  s[i] = 0;
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
    strcpy(s, "none");
  else
    s[i - 2] = 0;
  return s;
}

static void cmd_strip(struct userrec *u, int idx, char *par)
{
  char *nick = NULL, *changes = NULL, *c = NULL, s[2] = "";
  int dest = 0, i, pls, md, ok = 0;

  if (!par[0]) {
    dprintf(idx, STR("Your current strip settings are: %s (%s).\n"),
	    stripmasktype(dcc[idx].u.chat->strip_flags),
	    stripmaskname(dcc[idx].u.chat->strip_flags));
    return;
  }
  nick = newsplit(&par);
  if ((nick[0] != '+') && (nick[0] != '-') && u &&
      (u->flags & USER_MASTER)) {
    for (i = 0; i < dcc_total; i++)
      if (!egg_strcasecmp(nick, dcc[i].nick) && dcc[i].type == &DCC_CHAT &&
	  !ok) {
	ok = 1;
	dest = i;
      }
    if (!ok) {
      dprintf(idx, STR("No such user on the party line!\n"));
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
    putlog(LOG_CMDS, "*", STR("#%s# strip %s %s"), dcc[idx].nick, nick, changes);
  else
    putlog(LOG_CMDS, "*", STR("#%s# strip %s"), dcc[idx].nick, changes);
  if (dest == idx) {
    dprintf(idx, STR("Your strip settings are: %s (%s).\n"),
	    stripmasktype(dcc[idx].u.chat->strip_flags),
	    stripmaskname(dcc[idx].u.chat->strip_flags));
  } else {
    dprintf(idx, STR("Strip setting for %s: %s (%s).\n"), dcc[dest].nick,
	    stripmasktype(dcc[dest].u.chat->strip_flags),
	    stripmaskname(dcc[dest].u.chat->strip_flags));
    dprintf(dest, STR("%s set your strip settings to: %s (%s).\n"), dcc[idx].nick,
	    stripmasktype(dcc[dest].u.chat->strip_flags),
	    stripmaskname(dcc[dest].u.chat->strip_flags));
  }
  /* Set highlight flag here so user is able to control stripping of
   * bold also as intended -- dw 27/12/1999
   */
  console_dostore(dest);
}

static void cmd_su(struct userrec *u, int idx, char *par)
{
  int atr = u ? u->flags : 0, ok;
  struct flag_record fr = {FR_ANYWH | FR_CHAN | FR_GLOBAL, 0, 0, 0, 0, 0};

  u = get_user_by_handle(userlist, par);

  if (!par[0])
    dprintf(idx, STR("Usage: su <user>\n"));
  else if (!u)
    dprintf(idx, STR("No such user.\n"));
  else if (u->flags & USER_BOT)
    dprintf(idx, STR("You can't su to a bot... then again, why would you wanna?\n"));
  else if (dcc[idx].u.chat->su_nick)
    dprintf(idx, STR("You cannot currently double .su; try .su'ing directly.\n"));
  else {
    get_user_flagrec(u, &fr, NULL);
    ok = 1;
#ifdef HUB
    if (!glob_huba(fr))
      ok = 0;
#else /* !HUB */
    if (ischanhub() && !glob_chuba(fr))
      ok = 0;
#endif /* LEAF */
    if (!ok)
      dprintf(idx, STR("No party line access permitted for %s.\n"), par);
    else {
      correct_handle(par);
      putlog(LOG_CMDS, "*", STR("#%s# su %s"), dcc[idx].nick, par);
      if (!(atr & USER_OWNER) ||
	  ((u->flags & USER_OWNER) && (isowner(par)) &&
	   !(isowner(dcc[idx].nick)))) {
	/* This check is only important for non-owners */
	if (u_pass_match(u, "-")) {
	  dprintf(idx, STR("No password set for user. You may not .su to them.\n"));
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
        dcc[idx].u.chat->away = calloc(1, strlen(dcc[idx].nick) + 1);
	strcpy(dcc[idx].u.chat->away, dcc[idx].nick);
        dcc[idx].u.chat->su_nick = calloc(1, strlen(dcc[idx].nick) + 1);
	strcpy(dcc[idx].u.chat->su_nick, dcc[idx].nick);
	dcc[idx].user = u;
	strcpy(dcc[idx].nick, par);
	/* Display password prompt and turn off echo (send IAC WILL ECHO). */
	dprintf(idx, STR("Enter password for %s%s\n"), par,
		(dcc[idx].status & STAT_TELNET) ? TLN_IAC_C TLN_WILL_C
	       					  TLN_ECHO_C : "");
	dcc[idx].type = &DCC_CHAT_PASS;
      } else if (atr & USER_OWNER) {
	if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
	  botnet_send_part_idx(idx, "");
	chanout_but(-1, dcc[idx].u.chat->channel,
		    "*** %s left the party line.\n", dcc[idx].nick);
	dprintf(idx, STR("Setting your username to %s.\n"), par);
	if (atr & USER_MASTER)
	  dcc[idx].u.chat->con_flags = conmask;
        dcc[idx].u.chat->su_nick = calloc(1, strlen(dcc[idx].nick) + 1);
	strcpy(dcc[idx].u.chat->su_nick, dcc[idx].nick);
	dcc[idx].user = u;
	strcpy(dcc[idx].nick, par);
	dcc_chatter(idx);
      }
    }
  }
}

static void cmd_fixcodes(struct userrec *u, int idx, char *par)
{
  if (dcc[idx].status & STAT_TELNET) {
    dcc[idx].status |= STAT_ECHO;
    dcc[idx].status &= ~STAT_TELNET;
    dprintf(idx, STR("Turned off telnet codes.\n"));
    putlog(LOG_CMDS, "*", STR("#%s# fixcodes (telnet off)"), dcc[idx].nick);
  } else {
    dcc[idx].status |= STAT_TELNET;
    dcc[idx].status &= ~STAT_ECHO;
    dprintf(idx, STR("Turned on telnet codes.\n"));
    putlog(LOG_CMDS, "*", STR("#%s# fixcodes (telnet on)"), dcc[idx].nick);
  }
}

static void cmd_page(struct userrec *u, int idx, char *par)
{
  int a;

  if (!par[0]) {
    if (dcc[idx].status & STAT_PAGE) {
      dprintf(idx, STR("Currently paging outputs to %d lines.\n"),
	      dcc[idx].u.chat->max_line);
    } else
      dprintf(idx, STR("You don't have paging on.\n"));
    return;
  }
  a = atoi(par);
  if ((!a && !par[0]) || !egg_strcasecmp(par, "off")) {
    dcc[idx].status &= ~STAT_PAGE;
    dcc[idx].u.chat->max_line = 0x7ffffff;	/* flush_lines needs this */
    while (dcc[idx].u.chat->buffer)
      flush_lines(idx, dcc[idx].u.chat);
    dprintf(idx, STR("Paging turned off.\n"));
    putlog(LOG_CMDS, "*", STR("#%s# page off"), dcc[idx].nick);
  } else if (a > 0) {
    dprintf(idx, STR("Paging turned on, stopping every %d line%s.\n"), a,
        (a != 1) ? "s" : "");
    dcc[idx].status |= STAT_PAGE;
    dcc[idx].u.chat->max_line = a;
    dcc[idx].u.chat->line_count = 0;
    dcc[idx].u.chat->current_lines = 0;
    putlog(LOG_CMDS, "*", STR("#%s# page %d"), dcc[idx].nick, a);
  } else {
    dprintf(idx, STR("Usage: page <off or #>\n"));
    return;
  }
  console_dostore(idx);
}

#ifdef HUB
static void cmd_newleaf(struct userrec *u, int idx, char *par)
{
  char *handle = NULL, *host = NULL;

  putlog(LOG_CMDS, "*", STR("#%s# newleaf %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: newleaf <handle> [host] [anotherhost]\n"));
    dprintf(idx, STR("       Leafs can't link unless you specify a *!ident@ip hostmask\n"));
  } else {
    handle = newsplit(&par);
    if (strlen(handle) > HANDLEN)
      handle[HANDLEN] = 0;
    if (get_user_by_handle(userlist, handle))
      dprintf(idx, STR("Already got a %s user/bot\n"), handle);
    else if (strchr(BADHANDCHARS, handle[0]) != NULL)
      dprintf(idx, STR("You can't start a botnick with '%c'.\n"), handle[0]);
    else {
      struct userrec *u1 = NULL;
      struct bot_addr *bi = NULL;
      char tmp[81] = "";

      userlist = adduser(userlist, handle, STR("none"), "-", USER_BOT | USER_OP);
      u1 = get_user_by_handle(userlist, handle);
      bi = calloc(1, sizeof(struct bot_addr));

      bi->uplink = calloc(1, strlen(conf.bot->nick) + 1); 
/*      strcpy(bi->uplink, conf.bot->nick); */
      strcpy(bi->uplink, "");

      bi->address = calloc(1, 1);
      bi->telnet_port = 3333;
      bi->relay_port = 3333;
      bi->hublevel = 0;
      set_user(&USERENTRY_BOTADDR, u1, bi);
      host = newsplit(&par);
      while ((host) && (host[0])) {
        addhost_by_handle(handle, host);
        host = newsplit(&par);
      }
      /* set_user(&USERENTRY_PASS, u1, SALT2); */
      sprintf(tmp, STR("%lu %s"), now, u->handle);
      set_user(&USERENTRY_ADDED, u1, tmp);
      dprintf(idx, STR("Added new leaf: %s\n"), handle);
#ifdef HUB
      write_userfile(idx);
#endif /* HUB */
    }
  }
}

static void cmd_nopass(struct userrec *u, int idx, char *par)
{
  int cnt = 0;
  struct userrec *cu = NULL;
  char *users = NULL;

  users = malloc(1);

  putlog(LOG_CMDS, "*", "#%s# nopass %s", dcc[idx].nick, (par && par[0]) ? par : "");

  for (cu = userlist; cu; cu = cu->next) {
    if (!(cu->flags & USER_BOT)) {
      if (u_pass_match(cu, "-")) {
        cnt++;
        users = realloc(users, strlen(users) + strlen(cu->handle) + 1 + 1);
        strcat(users, cu->handle);
        strcat(users, " ");
      }
    }
  }
  if (!cnt) 
    dprintf(idx, "All users have passwords set.\n");
  else
    dprintf(idx, "Users without passwords: %s\n", users);
  free(users);
}
#endif /* HUB */

static void cmd_pls_ignore(struct userrec *u, int idx, char *par)
{
  char *who = NULL, s[UHOSTLEN] = "";
  unsigned long int expire_time = 0;

  if (!par[0]) {
    dprintf(idx, STR("Usage: +ignore <hostmask> [%%<XdXhXm>] [comment]\n"));
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
      simple_sprintf(s, "%s!*@*", who);
    else
      simple_sprintf(s, "*!%s", who);
  } else if (!strchr(who, '@'))
    simple_sprintf(s, "%s@*", who);
  else
    strcpy(s, who);

  if (match_ignore(s))
    dprintf(idx, STR("That already matches an existing ignore.\n"));
  else {
    dprintf(idx, STR("Now ignoring: %s (%s)\n"), s, par);
    addignore(s, dcc[idx].nick, par, expire_time ? now + expire_time : 0L);
    putlog(LOG_CMDS, "*", STR("#%s# +ignore %s %s"), dcc[idx].nick, s, par);
#ifdef HUB
    write_userfile(idx);
#endif /* HUB */
  }
}

static void cmd_mns_ignore(struct userrec *u, int idx, char *par)
{
  char buf[UHOSTLEN] = "";

  if (!par[0]) {
    dprintf(idx, STR("Usage: -ignore <hostmask | ignore #>\n"));
    return;
  }
  strncpyz(buf, par, sizeof buf);
  if (delignore(buf)) {
    putlog(LOG_CMDS, "*", STR("#%s# -ignore %s"), dcc[idx].nick, buf);
    dprintf(idx, STR("No longer ignoring: %s\n"), buf);
#ifdef HUB
    write_userfile(idx);
#endif /* HUB */
  } else
    dprintf(idx, STR("That ignore cannot be found.\n"));
}

static void cmd_ignores(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# ignores %s"), dcc[idx].nick, par);
  tell_ignores(idx, par);
}

static void cmd_pls_user(struct userrec *u, int idx, char *par)
{
  char *handle = NULL, *host = NULL;

  putlog(LOG_CMDS, "*", STR("#%s# +user %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: +user <handle> [hostmask]\n"));
    return;
  }
  handle = newsplit(&par);
  host = newsplit(&par);
  if (strlen(handle) > HANDLEN)
    handle[HANDLEN] = 0;
  if (get_user_by_handle(userlist, handle))
    dprintf(idx, STR("Someone already exists by that name.\n"));
  else if (strchr(BADNICKCHARS, handle[0]) != NULL)
    dprintf(idx, STR("You can't start a nick with '%c'.\n"), handle[0]);
  else if (!egg_strcasecmp(handle, conf.bot->nick))
    dprintf(idx, STR("Hey! That's MY name!\n"));
  else {
    struct userrec *u2 = NULL;
    char tmp[50] = "", s[16] = "", s2[17] = "";

    userlist = adduser(userlist, handle, host, "-", USER_DEFAULT);
    u2 = get_user_by_handle(userlist, handle);
    sprintf(tmp, STR("%lu %s"), now, u->handle);
    set_user(&USERENTRY_ADDED, u2, tmp);
    dprintf(idx, STR("Added %s (%s) with no flags.\n"), handle, host);
    while (par[0]) {
      host = newsplit(&par);
      set_user(&USERENTRY_HOSTS, u2, host);
      dprintf(idx, STR("Added host %s to %s.\n"), host, handle);
    }
    make_rand_str(s, 15);
    set_user(&USERENTRY_PASS, u2, s);

    make_rand_str(s2, 16);
    set_user(&USERENTRY_SECPASS, u2, s2);
    dprintf(idx, STR("%s's initial password set to \002%s\002\n"), handle, s);
    dprintf(idx, STR("%s's initial secpass set to \002%s\002\n"), handle, s2);

#ifdef HUB
    write_userfile(idx);
#endif /* HUB */
  }
}

static void cmd_mns_user(struct userrec *u, int idx, char *par)
{
  int idx2;
  char *handle = NULL;
  struct userrec *u2 = NULL;

  putlog(LOG_CMDS, "*", STR("#%s# -user %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: -user <hand>\n"));
    return;
  }
  handle = newsplit(&par);
  u2 = get_user_by_handle(userlist, handle);
  if (!u2 || !u) {
    dprintf(idx, STR("No such user!\n"));
    return;
  }
  if (isowner(u2->handle)) {
    dprintf(idx, STR("You can't remove a permanent bot owner!\n"));
    return;
  }
  if ((u2->flags & USER_ADMIN) && !(isowner(u->handle))) {
    dprintf(idx, STR("You can't remove an admin!\n"));
    return;
  }
  if ((u2->flags & USER_OWNER) && !(u->flags & USER_OWNER)) {
    dprintf(idx, STR("You can't remove a bot owner!\n"));
    return;
  }
  if (u2->flags & USER_BOT) {
    if ((bot_flags(u2) & BOT_SHARE) && !(u->flags & USER_OWNER)) {
      dprintf(idx, STR("You can't remove share bots.\n"));
      return;
    }
    for (idx2 = 0; idx2 < dcc_total; idx2++)
      if (dcc[idx2].type != &DCC_RELAY && dcc[idx2].type != &DCC_FORK_BOT &&
          !egg_strcasecmp(dcc[idx2].nick, handle))
        break;
    if (idx2 != dcc_total) {
      dprintf(idx, STR("You can't remove a directly linked bot.\n"));
      return;
    }
  }
  if (!(u->flags & USER_MASTER) &&
      !(u2->flags & USER_BOT)) {
    dprintf(idx, STR("You can't remove users who aren't bots!\n"));
    return;
  }
#ifdef LEAF
  check_this_user(handle, 1, NULL);
#endif /* LEAF */
  if (deluser(handle)) {
    dprintf(idx, STR("Deleted %s.\n"), handle);
#ifdef HUB
    write_userfile(idx);
#endif /* HUB */
  } else
    dprintf(idx, STR("Failed.\n"));
}

static void cmd_pls_host(struct userrec *u, int idx, char *par)
{
  char *handle = NULL, *host = NULL;
  struct userrec *u2 = NULL;
  struct list_type *q = NULL;
  struct flag_record fr2 = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0},
                     fr  = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0};

  putlog(LOG_CMDS, "*", STR("#%s# +host %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: +host [handle] <newhostmask>\n"));
    return;
  }

  handle = newsplit(&par);

  if (par[0]) {
    host = newsplit(&par);
    u2 = get_user_by_handle(userlist, handle);
  } else {
    host = handle;
    handle = dcc[idx].nick;
    u2 = u;
  }
  if (!u2 || !u) {
    dprintf(idx, STR("No such user.\n"));
    return;
  }
  get_user_flagrec(u, &fr, NULL);
  if (egg_strcasecmp(handle, dcc[idx].nick)) {
    get_user_flagrec(u2, &fr2, NULL);
    if (!glob_master(fr) && !glob_bot(fr2) && !chan_master(fr)) {
      dprintf(idx, STR("You can't add hostmasks to non-bots.\n"));
      return;
    }
    if (!glob_owner(fr) && glob_bot(fr2) && (bot_flags(u2) & BOT_SHARE)) {
      dprintf(idx, STR("You can't add hostmasks to share bots.\n"));
      return;
    }
    if (glob_admin(fr2) && !isowner(u->handle)) {
      dprintf(idx, STR("You can't add hostmasks to an admin.\n"));
      return;
    }
    if ((glob_owner(fr2) || glob_master(fr2)) && !glob_owner(fr)) {
      dprintf(idx, STR("You can't add hostmasks to a bot owner/master.\n"));
      return;
    }
    if ((chan_owner(fr2) || chan_master(fr2)) && !glob_master(fr) &&
        !glob_owner(fr) && !chan_owner(fr)) {
      dprintf(idx, STR("You can't add hostmasks to a channel owner/master.\n"));
      return;
    }
    if (!glob_master(fr) && !chan_master(fr)) {
      dprintf(idx, STR("Permission denied.\n"));
      return;
    }
  }
  if (!chan_master(fr) && get_user_by_host(host)) {
    dprintf(idx, STR("You cannot add a host matching another user!\n"));
    return;
  }
  
  for (q = get_user(&USERENTRY_HOSTS, u); q; q = q->next)
    if (!egg_strcasecmp(q->extra, host)) {
      dprintf(idx, STR("That hostmask is already there.\n"));
      return;
    }
  addhost_by_handle(handle, host);
  update_mod(handle, dcc[idx].nick, "+host", host);
  dprintf(idx, STR("Added '%s' to %s.\n"), host, handle);
#ifdef LEAF
  check_this_user(handle, 0, NULL);
#endif /* LEAF */
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
}

static void cmd_mns_host(struct userrec *u, int idx, char *par)
{
  char *handle = NULL, *host = NULL;
  struct userrec *u2 = NULL;
  struct flag_record fr2 = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0},
                     fr  = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0};

  putlog(LOG_CMDS, "*", STR("#%s# -host %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: -host [handle] <hostmask>\n"));
    return;
  }
  handle = newsplit(&par);
  if (par[0]) {
    host = newsplit(&par);
    u2 = get_user_by_handle(userlist, handle);
  } else {
    host = handle;
    handle = dcc[idx].nick;
    u2 = u;
  }
  if (!u2 || !u) {
    dprintf(idx, STR("No such user.\n"));
    return;
  }

  get_user_flagrec(u, &fr, NULL);
  get_user_flagrec(u2, &fr2, NULL);
  /* check to see if user is +d or +k and don't let them remove hosts */
  if (glob_deop(fr) || glob_kick(fr) || chan_deop(fr) || chan_kick (fr)) {
    dprintf(idx, STR("You can't remove hostmasks while having the +d or +k flag.\n"));
      return;
    }

  if (egg_strcasecmp(handle, dcc[idx].nick)) {
    if (!glob_master(fr) && !glob_bot(fr2) && !chan_master(fr)) {
      dprintf(idx, STR("You can't remove hostmasks from non-bots.\n"));
      return;
    }
    if (glob_bot(fr2) && (bot_flags(u2) & BOT_SHARE) && !glob_owner(fr)) {
      dprintf(idx, STR("You can't remove hostmasks from a share bot.\n"));
      return;
    }
    if (glob_admin(fr2) && !isowner(u->handle)) {
      dprintf(idx, STR("You can't remove hostmasks from an admin.\n"));
      return;
    }
    if ((glob_owner(fr2) || glob_master(fr2)) && !glob_owner(fr)) {
      dprintf(idx, STR("You can't remove hostmasks from a bot owner/master.\n"));
      return;
    }
    if ((chan_owner(fr2) || chan_master(fr2)) && !glob_master(fr) &&
        !glob_owner(fr) && !chan_owner(fr)) {
      dprintf(idx, STR("You can't remove hostmasks from a channel owner/master.\n"));
      return;
    }
    if (!glob_master(fr) && !chan_master(fr)) {
      dprintf(idx, STR("Permission denied.\n"));
      return;
    }
  }
  if (delhost_by_handle(handle, host)) {
    dprintf(idx, STR("Removed '%s' from %s.\n"), host, handle);
    update_mod(handle, dcc[idx].nick, "-host", host);
#ifdef LEAF
    check_this_user(handle, 2, host);
#endif /* LEAF */
#ifdef HUB
    write_userfile(idx);
#endif /* HUB */
  } else
    dprintf(idx, STR("Failed.\n"));
}

/* netserver */
static void cmd_netserver(struct userrec * u, int idx, char * par) {
  putlog(LOG_CMDS, "*", STR("#%s# netserver"), dcc[idx].nick);
  botnet_send_cmd_broad(-1, conf.bot->nick, u->handle, idx, STR("cursrv"));
}

static void cmd_botserver(struct userrec * u, int idx, char * par) {
  putlog(LOG_CMDS, "*", STR("#%s# botserver %s"), dcc[idx].nick, par);
  if (!par || !par[0]) {
    dprintf(idx, STR("Usage: botserver <bot>\n"));
    return;
  }
  if (nextbot(par)<0) {
    dprintf(idx, STR("%s isn't a linked bot\n"), par);
  }
  botnet_send_cmd(conf.bot->nick, par, u->handle, idx, STR("cursrv"));
}


void rcmd_cursrv(char * fbot, char * fhand, char * fidx) {
#ifdef LEAF
  char tmp[2048] = "";

  if (server_online)
    sprintf(tmp, "Currently: %-40s Lag: %d", cursrvname, server_lag);
  else
    sprintf(tmp, "Currently: none");

  botnet_send_cmdreply(conf.bot->nick, fbot, fhand, fidx, tmp);
#endif /* LEAF */
}

#ifdef HUB
/* netversion */
static void cmd_netversion(struct userrec * u, int idx, char * par) {
  putlog(LOG_CMDS, "*", STR("#%s# netversion"), dcc[idx].nick);
  botnet_send_cmd_broad(-1, conf.bot->nick, u->handle, idx, STR("ver"));
}

static void cmd_botversion(struct userrec * u, int idx, char * par) {
  putlog(LOG_CMDS, "*", STR("#%s# botversion %s"), dcc[idx].nick, par);
  if (!par || !par[0]) {
    dprintf(idx, STR("Usage: botversion <bot>\n"));
    return;
  }
  if (nextbot(par)<0) {
    dprintf(idx, STR("%s isn't a linked bot\n"), par);
  }
  botnet_send_cmd(conf.bot->nick, par, u->handle, idx, "ver");
}
#endif /* HUB */

void rcmd_ver(char * fbot, char * fhand, char * fidx) {
  char tmp[2048] = "";
  struct utsname un;

  sprintf(tmp, STR("%s "), version);
  if (uname(&un) < 0) {
    strcat(tmp, "(unknown OS)");
  } else {
    sprintf(tmp + strlen(tmp), "%s %s (%s)", un.sysname, un.release, un.machine);
  }
  botnet_send_cmdreply(conf.bot->nick, fbot, fhand, fidx, tmp);
}


/* netnick, botnick */
static void cmd_netnick (struct userrec *u, int idx, char *par) {
  putlog(LOG_CMDS, "*", STR("#%s# netnick"), dcc[idx].nick);
  botnet_send_cmd_broad(-1, conf.bot->nick, u->handle, idx, STR("curnick"));
}

static void cmd_botnick(struct userrec * u, int idx, char * par) {
  putlog(LOG_CMDS, "*", STR("#%s# botnick %s"), dcc[idx].nick, par);
  if (!par || !par[0]) {
    dprintf(idx, STR("Usage: botnick <bot>\n"));
    return;
  }
  if (nextbot(par) < 0) {
    dprintf(idx, STR("%s isn't a linked bot\n"), par);
  }
  botnet_send_cmd(conf.bot->nick, par, u->handle, idx, STR("curnick"));
}

void rcmd_curnick(char * fbot, char * fhand, char * fidx) {
#ifdef LEAF
  char tmp[1024] = "";

  if (server_online)
    sprintf(tmp, STR("Currently: %-20s "), botname);
  if (strncmp(botname, origbotname, strlen(botname)))
    sprintf(tmp, STR("%sWant: %s"), tmp, origbotname);
  if (!server_online)
    sprintf(tmp, STR("%s(not online)"), tmp);
  botnet_send_cmdreply(conf.bot->nick, fbot, fhand, fidx, tmp);
#endif /* LEAF */
}

/* netmsg, botmsg */
static void cmd_botmsg(struct userrec * u, int idx, char * par) {
  char *tnick = NULL, *tbot = NULL, tmp[1024] = "";

  putlog(LOG_CMDS, "*", STR("#%s# botmsg %s"), dcc[idx].nick, par);
  tbot = newsplit(&par);
  if (par[0])
    tnick = newsplit(&par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: botmsg <bot> <nick|#channel> <message>\n"));
    return;
  }
  if (nextbot(tbot)<0) {
    dprintf(idx, STR("No such bot linked\n"));
    return;
  }
  sprintf(tmp, STR("msg %s %s"), tnick, par);
  botnet_send_cmd(conf.bot->nick, tbot, u->handle, idx, tmp);
}

static void cmd_netmsg(struct userrec * u, int idx, char * par) {
  char *tnick = NULL, tmp[1024] = "";

  putlog(LOG_CMDS, "*", STR("#%s# netmsg %s"), dcc[idx].nick, par);
  tnick = newsplit(&par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: netmsg <nick|#channel> <message>\n"));
    return;
  }
  sprintf(tmp, STR("msg %s %s"), tnick, par);
  botnet_send_cmd_broad(-1, conf.bot->nick, u->handle, idx, tmp);
}

void rcmd_msg(char * tobot, char * frombot, char * fromhand, char * fromidx, char * par) {
#ifdef LEAF
  char buf[1024] = "", *nick = NULL;

  nick = newsplit(&par);
  dprintf(DP_SERVER, STR("PRIVMSG %s :%s\n"), nick, par);
  if (!strcmp(tobot, conf.bot->nick)) {
    sprintf(buf, STR("Sent message to %s"), nick);
    botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, buf);
  }
#endif /* LEAF */
}

#ifdef HUB
/* netlag */
static void cmd_netlag(struct userrec * u, int idx, char * par) {
  time_t tm;
  egg_timeval_t tv;
  char tmp[64] = "";

  putlog(LOG_CMDS, "*", STR("#%s# netlag"), dcc[idx].nick);
  
  timer_get_now(&tv);
  tm = (tv.sec % 10000) * 100 + (tv.usec * 100) / (1000000);
  sprintf(tmp, STR("ping %lu"), tm);
  dprintf(idx, STR("Sent ping to all linked bots\n"));
  botnet_send_cmd_broad(-1, conf.bot->nick, u->handle, idx, tmp);
}
#endif /* HUB */

void rcmd_ping(char * frombot, char *fromhand, char * fromidx, char * par) {
  char tmp[64] = "";

  sprintf(tmp, STR("pong %s"), par);
  botnet_send_cmd(conf.bot->nick, frombot, fromhand, atoi(fromidx), tmp);
}

void rcmd_pong(char *frombot, char *fromhand, char *fromidx, char *par) {
  int i = atoi(fromidx);

  if ((i >= 0) && (i < dcc_total) && (dcc[i].type == &DCC_CHAT) && (!strcmp(dcc[i].nick, fromhand))) {
    time_t tm;
    egg_timeval_t tv;

    timer_get_now(&tv);
    tm = ((tv.sec % 10000) * 100 + (tv.usec * 100) / (1000000)) - atoi(par);
    dprintf(i, STR("Pong from %s: %i.%i seconds\n"), frombot, (tm / 100), (tm % 100));
  }
}

/* exec commands */
#ifdef HUB
static void cmd_netw(struct userrec * u, int idx, char * par) {
  char tmp[128] = "";

  putlog(LOG_CMDS, "*", STR("#%s# netw"), dcc[idx].nick);
  strcpy(tmp, STR("exec w"));
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, tmp);
}

static void cmd_netps(struct userrec * u, int idx, char * par) {
  char buf[1024] = "";

  putlog(LOG_CMDS, "*", STR("#%s# netps %s"), dcc[idx].nick, par);
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    putlog(LOG_WARN, "*", STR("%s attempted 'netps' with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, STR("No."));
    return;
  }
  sprintf(buf, STR("exec ps %s"), par);
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, buf);
}

static void cmd_netlast(struct userrec * u, int idx, char * par) {
  char buf[1024] = "";

  putlog(LOG_CMDS, "*", STR("#%s# netlast %s"), dcc[idx].nick, par);
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    putlog(LOG_WARN, "*", STR("%s attempted 'netlast' with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, STR("No."));
    return;
  }
  sprintf(buf, STR("exec last %s"), par);
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, buf);
}
#endif /* HUB */

void crontab_show(struct userrec *u, int idx) {
  dprintf(idx, STR("Showing current crontab:\n"));
  if (!exec_str(idx, STR("crontab -l | grep -v \"^#\"")))
    dprintf(idx, STR("Exec failed"));
}

static void cmd_crontab(struct userrec *u, int idx, char *par) {
  char *code = NULL;
  int i;

  putlog(LOG_CMDS, "*", STR("#%s# crontab %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: crontab <status|delete|show|new> [interval]\n"));
    return;
  }
  code = newsplit(&par);
  if (!strcmp(code, STR("status"))) {
    i = crontab_exists();
    if (!i)
      dprintf(idx, STR("No crontab\n"));
    else if (i==1)
      dprintf(idx, STR("Crontabbed\n"));
    else
      dprintf(idx, STR("Error checking crontab status\n"));
  } else if (!strcmp(code, STR("show"))) {
    crontab_show(u, idx);
  } else if (!strcmp(code, STR("delete"))) {
    crontab_del();
    i = crontab_exists();
    if (!i)
      dprintf(idx, STR("No crontab\n"));
    else if (i==1)
      dprintf(idx, STR("Crontabbed\n"));
    else
      dprintf(idx, STR("Error checking crontab status\n"));
  } else if (!strcmp(code, STR("new"))) {
    i = atoi(par);
    if ((i <= 0) || (i > 60))
      i = 10;
    crontab_create(i);
    i = crontab_exists();
    if (!i)
      dprintf(idx, STR("No crontab\n"));
    else if (i == 1)
      dprintf(idx, STR("Crontabbed\n"));
    else
      dprintf(idx, STR("Error checking crontab status\n"));
  } else {
    dprintf(idx, STR("Usage: crontab status|delete|show|new [interval]\n"));
  }
}

#ifdef HUB
static void cmd_netcrontab(struct userrec * u, int idx, char * par) {
  char buf[1024] = "", *cmd = NULL;

  putlog(LOG_CMDS, "*", STR("#%s# netcrontab %s"), dcc[idx].nick, par);
  cmd = newsplit(&par);
  if ((strcmp(cmd, STR("status")) && strcmp(cmd, STR("show")) && strcmp(cmd, STR("delete")) && strcmp(cmd, STR("new")))) {
    dprintf(idx, STR("Usage: netcrontab <status|delete|show|new> [interval]\n"));
    return;
  }
  egg_snprintf(buf, sizeof buf, STR("exec crontab %s %s"), cmd, par);
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, buf);
}
#endif /* HUB */

void rcmd_exec(char * frombot, char * fromhand, char * fromidx, char * par) {
  char *cmd = NULL, scmd[512] = "", *out = NULL, *err = NULL;

  cmd = newsplit(&par);
  scmd[0] = 0;
  if (!strcmp(cmd, "w")) {
    strcpy(scmd, "w");
  } else if (!strcmp(cmd, STR("last"))) {
    char user[20] = "";

    if (par[0]) {
      strncpyz(user, par, sizeof(user));
    } else {
      strncpyz(user, conf.username, sizeof(user));
    }
    if (!user[0]) {
      botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, STR("Can't determine user id for process"));
      return;
    }
    sprintf(scmd, STR("last %s"), user);
  } else if (!strcmp(cmd, STR("ps"))) {
    sprintf(scmd, STR("ps %s"), par);
  } else if (!strcmp(cmd, STR("raw"))) {
    sprintf(scmd, STR("%s"), par);
  } else if (!strcmp(cmd, STR("kill"))) {
    sprintf(scmd, STR("kill %s"), par);
  } else if (!strcmp(cmd, STR("crontab"))) {
    char *code = newsplit(&par);

    scmd[0] = 0;
    if (!strcmp(code, STR("show"))) {
      strcpy(scmd, STR("crontab -l | grep -v \"^#\""));
    } else if (!strcmp(code, STR("delete"))) {
      crontab_del();
    } else if (!strcmp(code, STR("new"))) {
      int i=atoi(par);
      if ((i <= 0) || (i > 60))
        i = 10;
      crontab_create(i);
    }
    if (!scmd[0]) {
      char s[200] = "";
      int i = crontab_exists();

      if (!i)
        sprintf(s, STR("No crontab"));
      else if (i == 1)
        sprintf(s, STR("Crontabbed"));
      else
        sprintf(s, STR("Error checking crontab status"));
      botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, s);
    }
  }
  if (!scmd[0])
    return;
  if (shell_exec(scmd, NULL, &out, &err)) {
    if (out) {
      char *p = NULL, *np = NULL;

      botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, STR("Result:"));
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

      botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, STR("Errors:"));
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
    botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, STR("exec failed"));
  }

}

static void cmd_botjump(struct userrec * u, int idx, char * par) {
  char *tbot = NULL, buf[1024] = "";

  putlog(LOG_CMDS, "*", STR("#%s# botjump %s"), dcc[idx].nick, par);
  tbot = newsplit(&par);
  if (!tbot[0]) {
    dprintf(idx, STR("Usage: botjump <bot> [server [port [pass]]]\n"));
    return;
  }
  if (nextbot(tbot)<0) {
    dprintf(idx, STR("No such linked bot\n"));
    return;
  }
  sprintf(buf, STR("jump %s"), par);
  botnet_send_cmd(conf.bot->nick, tbot, dcc[idx].nick, idx, buf);
}

void rcmd_jump(char * frombot, char * fromhand, char * fromidx, char * par) {
#ifdef LEAF
  char *other = NULL;
  int port;

  if (par[0]) {
    other = newsplit(&par);
    port = atoi(newsplit(&par));
    if (!port)
      port = default_port;
    strncpyz(newserver, other, 120); //newserver
    newserverport = port; //newserverport
    strncpyz(newserverpass, par, 120); //newserverpass
  }
  botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, STR("Jumping..."));

  nuke_server("Jumping...");
  cycle_time = 0;
#endif /* LEAF */
}


/* "Remotable" commands */
void gotremotecmd (char *forbot, char *frombot, char *fromhand, char *fromidx, char *cmd) {
  char *par = cmd;

  cmd = newsplit(&par);
  if (!strcmp(cmd, STR("exec"))) {
    rcmd_exec(frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, STR("curnick"))) {
    rcmd_curnick(frombot, fromhand, fromidx);
  } else if (!strcmp(cmd, STR("cursrv"))) {
    rcmd_cursrv(frombot, fromhand, fromidx);
  } else if (!strcmp(cmd, STR("jump"))) {
    rcmd_jump(frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, STR("msg"))) {
    rcmd_msg(forbot, frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, STR("ver"))) {
    rcmd_ver(frombot, fromhand, fromidx);
  } else if (!strcmp(cmd, STR("ping"))) {
    rcmd_ping(frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, STR("pong"))) {
    rcmd_pong(frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, STR("die"))) {
    exit(0);
  } else {
    botnet_send_cmdreply(conf.bot->nick, frombot, fromhand, fromidx, STR("Unrecognized remote command"));
  }
}
    
void gotremotereply (char *frombot, char *tohand, char *toidx, char *ln) {
  int idx = atoi(toidx);

  if ((idx >= 0) && (idx < dcc_total) && (dcc[idx].type == &DCC_CHAT) && (!strcmp(dcc[idx].nick, tohand))) {
    char *buf = NULL;
    
    buf = malloc(strlen(frombot) + 2 + 1);

    sprintf(buf, "(%s)", frombot);
    dprintf(idx, STR("%-13s %s\n"), buf, ln);
    free(buf);
  }
}

static void cmd_traffic(struct userrec *u, int idx, char *par)
{
  unsigned long itmp, itmp2;

  dprintf(idx, STR("Traffic since last restart\n"));
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

  sprintf(unit, "Bytes");
  xbytes = bytes;
  if (xbytes > 1024.0) {
    sprintf(unit, "KBytes");
    xbytes = xbytes / 1024.0;
  }
  if (xbytes > 1024.0) {
    sprintf(unit, "MBytes");
    xbytes = xbytes / 1024.0;
  }
  if (xbytes > 1024.0) {
    sprintf(unit, "GBytes");
    xbytes = xbytes / 1024.0;
  }
  if (xbytes > 1024.0) {
    sprintf(unit, "TBytes");
    xbytes = xbytes / 1024.0;
  }
  if (bytes > 1024)
    sprintf(traffictxt, "%.2f %s", xbytes, unit);
  else
    sprintf(traffictxt, "%lu Bytes", bytes);
  return traffictxt;
}

static void cmd_whoami(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# whoami"), dcc[idx].nick);
  dprintf(idx, "You are %s@%s.\n", dcc[idx].nick, conf.bot->nick);
}

static void cmd_quit(struct userrec *u, int idx, char *text)
{
	if (dcc[idx].u.chat->channel >= 0 && dcc[idx].u.chat->channel < GLOBAL_CHANS) {
		check_bind_chpt(conf.bot->nick, dcc[idx].nick, dcc[idx].sock, dcc[idx].u.chat->channel);
	}
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
		strcpy(dcc[idx].nick, dcc[idx].u.chat->su_nick);
		dcc[idx].type = &DCC_CHAT;
		dprintf(idx, "Returning to real nick %s!\n", dcc[idx].u.chat->su_nick);
		free(dcc[idx].u.chat->su_nick);
		dcc[idx].u.chat->su_nick = NULL;
		dcc_chatter(idx);

		if (dcc[idx].u.chat->channel < GLOBAL_CHANS && dcc[idx].u.chat->channel >= 0) {
			botnet_send_join_idx(idx, -1);
		}
	} else if ((dcc[idx].sock != STDOUT) || backgrd) {
		killsock(dcc[idx].sock);
		lostdcc(idx);
	} else {
		dprintf(DP_STDOUT, "\n### SIMULATION RESET\n\n");
		dcc_chatter(idx);
	}
}

/* DCC CHAT COMMANDS
 */
/* Function call should be:
 *   int cmd_whatever(idx,"parameters");
 * As with msg commands, function is responsible for any logging.
 */
cmd_t C_dcc[] =
{
  {"+host",		"m|m",	(Function) cmd_pls_host,	NULL},
  {"+ignore",		"m",	(Function) cmd_pls_ignore,	NULL},
  {"+user",		"m",	(Function) cmd_pls_user,	NULL},
#ifdef HUB
  {"-bot",		"a",	(Function) cmd_mns_user,	NULL},
#endif /* HUB */
  {"-host",		"",	(Function) cmd_mns_host,	NULL},
  {"-ignore",		"m",	(Function) cmd_mns_ignore,	NULL},
  {"-user",		"m",	(Function) cmd_mns_user,	NULL},
  {"addlog",		"mo|o",	(Function) cmd_addlog,		NULL},
/*  {"putlog",		"mo|o",	(Function) cmd_addlog,		NULL}, */
  {"about",		"",	(Function) cmd_about,		NULL},
  {"away",		"",	(Function) cmd_away,		NULL},
  {"back",		"",	(Function) cmd_back,		NULL},
#ifdef HUB
  {"backup",		"m|m",	(Function) cmd_backup,		NULL},
  {"boot",		"m",	(Function) cmd_boot,		NULL},
  {"botconfig",		"n",	(Function) cmd_botconfig,	NULL},
  {"botinfo",		"",	(Function) cmd_botinfo,		NULL},
  {"bots",		"m",	(Function) cmd_bots,		NULL},
  {"downbots",		"m",	(Function) cmd_downbots,	NULL},
  {"bottree",		"n",	(Function) cmd_bottree,		NULL},
  {"chaddr",		"a",	(Function) cmd_chaddr,		NULL},
#endif /* HUB */
  {"chat",		"",	(Function) cmd_chat,		NULL},
  {"chattr",		"m|m",	(Function) cmd_chattr,		NULL},
#ifdef HUB
  {"chhandle",		"m",	(Function) cmd_chhandle,	NULL},
/*  {"chnick",		"m",	(Function) cmd_chhandle,	NULL}, */
  {"chpass",		"m",	(Function) cmd_chpass,		NULL},
  {"chsecpass",		"n",	(Function) cmd_chsecpass,	NULL},
#ifdef S_DCCPASS
  {"cmdpass",           "a",    (Function) cmd_cmdpass,         NULL},
#endif /* S_DCCPASS */
#endif /* HUB */
  {"color",		"",     (Function) cmd_color,           NULL},
  {"comment",		"m|m",	(Function) cmd_comment,		NULL},
#ifdef HUB
  {"config",		"n",	(Function) cmd_config,		NULL},
#endif /* HUB */
  {"console",		"-|-",	(Function) cmd_console,		NULL},
  {"date",		"",	(Function) cmd_date,		NULL},
  {"dccstat",		"a",	(Function) cmd_dccstat,		NULL},
  {"debug",		"a",	(Function) cmd_debug,		NULL},
  {"timers",		"a",	(Function) cmd_timers,		NULL},
  {"die",		"n",	(Function) cmd_die,		NULL},
  {"echo",		"",	(Function) cmd_echo,		NULL},
  {"fixcodes",		"",	(Function) cmd_fixcodes,	NULL},
  {"handle",		"",	(Function) cmd_handle,		NULL},
  {"nohelp",		"-|-",	(Function) cmd_nohelp,		NULL},
  {"help",		"-|-",	(Function) cmd_help,		NULL},
  {"ignores",		"m",	(Function) cmd_ignores,		NULL},
#ifdef HUB
  {"link",		"n",	(Function) cmd_link,		NULL},
#endif /* HUB */
  {"match",		"m|m",	(Function) cmd_match,		NULL},
  {"me",		"",	(Function) cmd_me,		NULL},
  {"motd",		"",	(Function) cmd_motd,		NULL},
#ifdef HUB
  {"newleaf",		"n",	(Function) cmd_newleaf,		NULL},
  {"nopass",		"m",	(Function) cmd_nopass,		NULL},
#endif /* HUB */
  {"newpass",		"",	(Function) cmd_newpass,		NULL},
  {"secpass",		"",	(Function) cmd_secpass,		NULL},
/*  {"nick",		"",	(Function) cmd_handle,		NULL}, */
  {"page",		"",	(Function) cmd_page,		NULL},
  {"quit",		"",	(Function) cmd_quit,		NULL},
  {"relay",		"i",	(Function) cmd_relay,		NULL},
#ifdef HUB
  {"reload",		"m|m",	(Function) cmd_reload,		NULL},
#endif /* HUB */
  {"restart",		"m",	(Function) cmd_restart,		NULL},
#ifdef HUB
  {"save",		"m|m",	(Function) cmd_save,		NULL},
#endif /* HUB */
  {"simul",		"a",	(Function) cmd_simul,		NULL},
  {"status",		"m|m",	(Function) cmd_status,		NULL},
  {"strip",		"",	(Function) cmd_strip,		NULL},
  {"su",		"a",	(Function) cmd_su,		NULL},
#ifdef HUB
  {"trace",		"n",	(Function) cmd_trace,		NULL},
#endif /* HUB */
  {"traffic",		"m",	(Function) cmd_traffic,		NULL},
  {"unlink",		"m",	(Function) cmd_unlink,		NULL},
  {"update",		"a",	(Function) cmd_update,		NULL},
#ifdef HUB
  {"netcrontab",	"a",	(Function) cmd_netcrontab,	NULL},
#endif /* HUB */
  {"uptime",		"m|m",	(Function) cmd_uptime,		NULL},
  {"crontab",		"a",	(Function) cmd_crontab,		NULL},
#ifdef HUB
  {"vbottree",		"n",	(Function) cmd_vbottree,	NULL},
  {"who",		"n",	(Function) cmd_who,		NULL},
#endif /* HUB */
  {"whois",		"",	(Function) cmd_whois,		NULL},
  {"whom",		"",	(Function) cmd_whom,		NULL},
  {"whoami",		"",	(Function) cmd_whoami,		NULL},
  {"botjump",           "m",    (Function) cmd_botjump,         NULL},
  {"botmsg",		"o",    (Function) cmd_botmsg,          NULL},
  {"netmsg", 		"n", 	(Function) cmd_netmsg, 		NULL},
  {"botnick", 		"m", 	(Function) cmd_botnick, 	NULL},
  {"netnick", 		"m", 	(Function) cmd_netnick, 	NULL},
#ifdef HUB
  {"netw", 		"n", 	(Function) cmd_netw, 		NULL},
  {"netps", 		"n", 	(Function) cmd_netps, 		NULL},
  {"netlast", 		"n", 	(Function) cmd_netlast, 	NULL},
  {"netlag", 		"m", 	(Function) cmd_netlag, 		NULL},
#endif /* HUB */
  {"botserver",		"m",	(Function) cmd_botserver,	NULL},
  {"netserver", 	"m", 	(Function) cmd_netserver, 	NULL},
#ifdef HUB
  {"botversion", 	"o", 	(Function) cmd_botversion, 	NULL},
  {"netversion", 	"o", 	(Function) cmd_netversion, 	NULL},
#endif /* HUB */
  {"userlist", 		"m", 	(Function) cmd_userlist, 	NULL},
  {"ps", 		"n", 	(Function) cmd_ps, 		NULL},
  {"last", 		"n", 	(Function) cmd_last, 		NULL},
  {"exec", 		"a", 	(Function) cmd_exec, 		NULL},
  {"w", 		"n", 	(Function) cmd_w, 		NULL},
  {"channels", 		"", 	(Function) cmd_channels, 	NULL},
  {"randstring", 	"", 	(Function) cmd_randstring, 	NULL},
#ifdef HUB
  {"botcmd",		"i",	(Function) cmd_botcmd, 		NULL},
  {"bc",		"i",	(Function) cmd_botcmd, 		NULL},
  {"hublevel", 		"a", 	(Function) cmd_hublevel, 	NULL},
  {"lagged", 		"m", 	(Function) cmd_lagged, 		NULL},
  {"uplink", 		"a", 	(Function) cmd_uplink, 		NULL},
#endif /* HUB */
  {NULL,		NULL,	NULL,				NULL}
};
