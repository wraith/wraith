/*
 * cmds.c -- handles:
 *   commands from a user via dcc
 *   (split in 3, this portion contains no-irc commands)
 *
 */

#include "main.h"
#include "tandem.h"
#include "modules.h"
#include <ctype.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/types.h>
       
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif

extern struct chanset_t	 *chanset;
extern struct dcc_t	 *dcc;
extern struct userrec	 *userlist;
extern tcl_timer_t	 *timer, *utimer;
extern int		 dcc_total, remote_boots, backgrd, 
			 do_restart, conmask, must_be_owner,
			 strict_host, quiet_save, cfg_count;

extern unsigned long	 otraffic_irc, otraffic_irc_today,
			 itraffic_irc, itraffic_irc_today,
			 otraffic_bn, otraffic_bn_today,
			 itraffic_bn, itraffic_bn_today,
			 otraffic_dcc, otraffic_dcc_today,
			 itraffic_dcc, itraffic_dcc_today,
			 otraffic_trans, otraffic_trans_today,
			 itraffic_trans, itraffic_trans_today,
			 otraffic_unknown, otraffic_unknown_today,
			 itraffic_unknown, itraffic_unknown_today;
extern Tcl_Interp 	 *interp;
extern char		 botnetnick[], origbotname[], ver[], network[],
			 owner[], quit_msg[], dcc_prefix[], 
                         botname[], *binname, egg_version[];
extern time_t		 now, online_since, buildts;
extern module_entry	 *module_list;
extern struct cfg_entry  CFG_MOTD, **cfg;
extern tand_t             *tandbot;

static char		 *btos(unsigned long);
mycmds 			 cmds[500]; //the list of dcc cmds for help system
int    			 cmdi = 0;

#ifdef HUB
static void tell_who(struct userrec *u, int idx, int chan)
{
  int i, k, ok = 0, atr = u ? u->flags : 0;
  int nicklen;
  char format[81];
  char s[1024];			/* temp fix - 1.4 has a better one */

  if (!chan)
    dprintf(idx, "%s  (* = %s, + = %s, @ = %s)\n",
		BOT_PARTYMEMBS, MISC_OWNER, MISC_MASTER, MISC_OP);
  else {
    simple_sprintf(s, "assoc %d", chan);
    if ((Tcl_Eval(interp, s) != TCL_OK) || !interp->result[0])
      dprintf(idx, "%s %s%d:  (* = %s, + = %s, @ = %s)\n",
		       BOT_PEOPLEONCHAN,
		       (chan < GLOBAL_CHANS) ? "" : "*",
		       chan % GLOBAL_CHANS,
		       MISC_OWNER, MISC_MASTER, MISC_OP);
    else
      dprintf(idx, "%s '%s' (%s%d):  (* = %s, + = %s, @ = %s)\n",
		       BOT_PEOPLEONCHAN, interp->result,
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
      egg_strftime(s, 14, "%d %b %H:%M", localtime(&dcc[i].timeval));
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
  char s[512], s2[32];
  struct chanset_t *chan;
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
  simple_sprintf(s, "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, botnetnick);
  botnet_send_infoq(-1, s);
  s[0] = 0;
  if (module_find("server", 0, 0)) {
    for (chan = chanset; chan; chan = chan->next) { 
      if (!channel_secret(chan)) {
	if ((strlen(s) + strlen(chan->dname) + strlen(network)
                   + strlen(botnetnick) + strlen(ver) + 1) >= 490) {
          strcat(s,"++  ");
          break; /* yeesh! */
	}
	strcat(s, chan->dname);
	strcat(s, ", ");
      }
    }

    if (s[0]) {
      s[strlen(s) - 2] = 0;
      dprintf(idx, "*** [%s] %s <%s> (%s) [UP %s]\n", botnetnick,
	      ver, network, s, s2);
    } else
      dprintf(idx, "*** [%s] %s <%s> (%s) [UP %s]\n", botnetnick,
	      ver, network, BOT_NOCHANNELS, s2);
  } else
    dprintf(idx, STR("*** [%s] %s <NO_IRC> [UP %s]\n"), botnetnick, ver, s2);
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
      Tcl_SetVar(interp, "chan", par, 0);
      if ((Tcl_VarEval(interp, "assoc ", "$chan", NULL) == TCL_OK) &&
	  interp->result[0]) {
	chan = atoi(interp->result);
      }
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
static void cmd_config(struct userrec *u, int idx, char *par)
{
  /*
     .config
     Usage + available entry list
     .config name
     Show current value + description
     .config name value
     Set
   */
  char *name;
  struct cfg_entry *cfgent = NULL;
  int cnt, i;

  putlog(LOG_CMDS, "*", STR("#%s# config %s"), dcc[idx].nick, par);
  if (!par[0]) {
    char *outbuf = nmalloc(1);
    outbuf[0] = 0;
    dprintf(idx, STR("Usage: config [name [value|-]]\n"));
    dprintf(idx, STR("Defined config entry names:\n"));
    cnt = 0;
    for (i=0;i<cfg_count;i++) {
      if ((cfg[i]->flags & CFGF_GLOBAL) && (cfg[i]->describe)) {
	if (!cnt) {
          outbuf = nrealloc(outbuf, 2 + 1);
	  sprintf(outbuf, "  ");
        }
        outbuf = nrealloc(outbuf, strlen(outbuf) + strlen(cfg[i]->name) + 1 + 1);
	sprintf(outbuf, STR("%s%s "), outbuf, cfg[i]->name);
	cnt++;
	if (cnt == 10) {
	  dprintf(idx, "%s\n", outbuf);
	  cnt=0;
	}
      }
    }
    if (cnt)
      dprintf(idx, "%s\n", outbuf);
    if (outbuf)
      nfree(outbuf);
    return;
  }
  name = newsplit(&par);
  for (i = 0; !cfgent && (i < cfg_count); i++)
    if (!strcmp(cfg[i]->name, name))
      cfgent=cfg[i];
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
  struct userrec *u2;
  char *p;
  struct xtra_key *k;
  struct cfg_entry *cfgent;
  int i, cnt;

  /* botconfig bot [name [value]]  */
  putlog(LOG_CMDS, "*", STR("#%s# botconfig %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: botconfig bot [name [value|-]]\n"));
    cnt=0;
    for (i=0; i < cfg_count; i++) {
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
	  dprintf(idx, STR("  %s: %s\n"), k->key, k->data);
	else
	  dprintf(idx, STR("  %s: (not set)\n"), cfg[i]->name);
      }
    }
    return;
  }
  p = newsplit(&par);
  cfgent=NULL;
  for (i = 0; !cfgent && (i < cfg_count); i++)
    if (!strcmp(cfg[i]->name, p) && (cfg[i]->flags & CFGF_LOCAL) && (cfg[i]->describe))
      cfgent=cfg[i];
  if (!cfgent) {
    dprintf(idx, STR("No such configuration value\n"));
    return;
  }
  if (par[0]) {
    char tmp[100];    
    set_cfg_str(u2->handle, cfgent->name, (strcmp(par, "-")) ? par : NULL);
    snprintf(tmp, sizeof tmp, "%s %s", cfgent->name, par);
    update_mod(u2->handle, dcc[idx].nick, "botconfig", tmp);
    dprintf(idx, STR("Now: "));
    write_userfile(idx);
  } else {
    if (cfgent->describe)
      cfgent->describe(cfgent, idx);
  }
  k = get_user(&USERENTRY_CONFIG, u2);
  while (k && strcmp(k->key, cfgent->name))
    k=k->next;
  if (k)
    dprintf(idx, STR("  %s: %s\n"), k->key, k->data);
  else
    dprintf(idx, STR("  %s: (not set)\n"), cfgent->name);
}

#ifdef S_DCCPASS
static void cmd_cmdpass(struct userrec *u, int idx, char *par)
{
  struct tcl_bind_mask_b *hm;
  char *cmd = NULL, *pass = NULL;
  int i, l = 0;

  /* cmdpass [command [newpass]] */
  cmd = newsplit(&par);
  putlog(LOG_CMDS, "*", STR("#%s# cmdpass %s ..."), dcc[idx].nick, cmd[0] ? cmd : "");
  pass = newsplit(&par);
  if (!cmd[0] || par[0]) {
    dprintf(idx, STR("Usage: %scmdpass command [password]\n"), dcc_prefix);
    dprintf(idx, STR("  if no password is specified, the commands password is reset\n"));
    return;
  }
  for (i = 0; cmd[i]; i++)
    cmd[i] = tolower(cmd[i]);

  if (!egg_strcasecmp(cmd, "op")) l++;
  else if (!egg_strcasecmp(cmd, "act")) l++;
  else if (!egg_strcasecmp(cmd, "adduser")) l++;
  else if (!egg_strcasecmp(cmd, "channel")) l++;
  else if (!egg_strcasecmp(cmd, "deluser")) l++;
  else if (!egg_strcasecmp(cmd, "deop")) l++;
  else if (!egg_strcasecmp(cmd, "devoice")) l++;
  else if (!egg_strcasecmp(cmd, "getkey")) l++;
  else if (!egg_strcasecmp(cmd, "find")) l++;
  else if (!egg_strcasecmp(cmd, "invite")) l++;
  else if (!egg_strcasecmp(cmd, "kick")) l++;
  else if (!egg_strcasecmp(cmd, "kickban")) l++;
  else if (!egg_strcasecmp(cmd, "mdop")) l++;
  else if (!egg_strcasecmp(cmd, "msg")) l++;
  else if (!egg_strcasecmp(cmd, "reset")) l++;
  else if (!egg_strcasecmp(cmd, "resetbans")) l++;
  else if (!egg_strcasecmp(cmd, "resetexempts")) l++;
  else if (!egg_strcasecmp(cmd, "resetinvites")) l++;
  else if (!egg_strcasecmp(cmd, "say")) l++;
  else if (!egg_strcasecmp(cmd, "topic")) l++;
  else if (!egg_strcasecmp(cmd, "voice")) l++;
  else if (!egg_strcasecmp(cmd, "clearqueue")) l++;
  else if (!egg_strcasecmp(cmd, "dump")) l++;
  else if (!egg_strcasecmp(cmd, "jump")) l++;
  else if (!egg_strcasecmp(cmd, "servers")) l++;
  else if (!egg_strcasecmp(cmd, "authed")) l++;

  if (!l) {
    for (hm = H_dcc->first; hm; hm = hm->next)
      if (!egg_strcasecmp(cmd, hm->mask))
        break;
    if (!hm) {
      dprintf(idx, STR("No such DCC command\n"));
      return;
    }
  }
  if (pass[0]) {
    char epass[36], tmp[256];
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
      dprintf(idx, STR("Set command password for %s\n"), cmd);
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
    write_userfile(idx);
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
  botnet_send_act(idx, botnetnick, dcc[idx].nick,
		  dcc[idx].u.chat->channel, par);
  check_tcl_act(dcc[idx].nick, dcc[idx].u.chat->channel, par);
}

static void cmd_motd(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# motd %s"), dcc[idx].nick, par);
  if (par[0] && (u->flags & USER_MASTER)) {
    char *s;

    s = nmalloc(strlen(par) + 1 + HANDLEN + 1); /* +1: ' ' */
    sprintf(s, STR("%s %s"), dcc[idx].nick, par);
    set_cfg_str(NULL, "motd", s);
    nfree(s);
    dprintf(idx, STR("Motd set\n"));
  } else {
    show_motd(idx);
  }
}

static void cmd_about(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# about"), dcc[idx].nick);
  dprintf(idx, STR("Wraith (%s) botpack by bryan, CVS date %lu"), buildts);
  dprintf(idx, STR("..with credits and thanks to the following:\n"), egg_version);
  dprintf(idx, STR("(written from a base of Eggdrop 1.6.12)\n\n"));
  dprintf(idx, STR("Eggdrop team for developing such a great bot to code off of.\n"));
  dprintf(idx, STR("Einride and ievil for taking eggdrop1.4.3 and making their very effecient botpack Ghost.\n"));
  dprintf(idx, STR("SFC for providing compile shells, continuous input, feature suggestions, and testing.\n"));
  dprintf(idx, STR("xmage for beta testing.\n"));
  dprintf(idx, STR("ryguy for beta testing, providing code, finding bugs, and providing input.\n"));
  dprintf(idx, STR("passwd for beta testing, and his dedication to finding bugs.\n"));
  dprintf(idx, STR("pgpkeys for suggestions.\n"));
  dprintf(idx, STR("qFox for providing an mIRC $md5() alias, not requiring a dll or >6.03\n"));
  dprintf(idx, STR("Sith_Lord helping test ipv6 on the bot (admin@elitepackets.com)\n"));
  dprintf(idx, STR("Excelsior for finding a bug on BSD with the ipv6.\n"));
  dprintf(idx, STR("syt for giving me inspiration to code a more secure bot.\n\n\n"));
  dprintf(idx, STR("Mystikal for finding various bugs.\n"));
  dprintf(idx, STR("Blackjac for helping with the bx auth script with his Sentinel script.\n"));
  dprintf(idx, STR("The following botpacks gave me inspiration and ideas (no code):\n"));
  dprintf(idx, STR("awptic by lordoptic\n"));
  dprintf(idx, STR("optikz by ryguy and lordoptic\n"));
  dprintf(idx, STR("celdrop by excelsior\n"));
  dprintf(idx, STR("genocide by various\n"));
  dprintf(idx, STR("tfbot by warknight and loslinux\n"));
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
  char *new, pass[16];

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
}

static void cmd_secpass(struct userrec *u, int idx, char *par)
{
  char *new, pass[17];

  putlog(LOG_CMDS, "*", STR("#%s# secpass..."), dcc[idx].nick);
  if (!par[0]) {
    dprintf(idx, STR("Usage: secpass <newsecpass>\nIf you use \"rand\" as the secpass, a random pass will be chosen.\n"));
    return;
  }
  new = newsplit(&par);

  if (!strcmp(new, "rand")) {
    make_rand_str(pass, 16);
  } else {
    if (strlen(new) < 6) {
      dprintf(idx, STR("Please use at least 6 characters.\n"));
      return;
    } else {
      sprintf(pass, "%s", new);
    }
  }
  if (strlen(pass) > 16)
    pass[16] = 0;
  set_user(&USERENTRY_SECPASS, u, pass);
  dprintf(idx, STR("Changed your secpass to: %s.\n"), pass);
}

#ifdef HUB
static void cmd_bots(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# bots"), dcc[idx].nick);
  tell_bots(idx);
}

static void cmd_downbots(struct userrec *u, int idx, char *par)
{
  struct userrec *u2;
  int cnt = 0, tot = 0;
  char work[128] = "";

  putlog(LOG_CMDS, "*", STR("#%s# downbots"), dcc[idx].nick);
  for (u2 = userlist; u2; u2 = u2->next) {
    if (u2->flags & USER_BOT) {
      if (egg_strcasecmp(u2->handle, botnetnick)) {
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

static void cmd_help(struct userrec *u, int idx, char *par)
{
  char flg[100];
  int i = 0,
    showall = 0,
    fnd = 0,
    n = 0,
    done = 0,
    first = 0, 
    o = 0,
    end;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  char *fcats, *flag, temp[100], buf[2046];

Context;
  egg_snprintf(temp, sizeof temp, "a|- a|a n|- n|n m|- m|m mo|o m|o i|- o|o o|- p|- -|-");
  fcats = temp;

  putlog(LOG_CMDS, "*", STR("#%s# help %s"), dcc[idx].nick, par);
  get_user_flagrec(u, &fr, dcc[idx].u.chat->con_chan);
  if (!par[0]) {
    showall = 1;
    build_flags(flg, &fr, NULL);
    dprintf(idx, STR("Showing help topics matching your flags: (%s)\n  "), flg);
  } else {
    dprintf(idx, "Not yet implemented.\n");
    return;
  }

  for (o = 0; o < cmdi; o++)
  ;
  /* this displays help for old system
  {
    if (!flagrec_ok(&cmds[o].flags, &fr))
      continue;
    if (!showall && !egg_strcasecmp(par, cmds[o].name)) {
      fnd = 1;
      build_flags(flg, &(cmds[o].flags), NULL);
      dprintf(idx, STR("### %s (required flags: %s)\n"), cmds[o].name, flg);
      dprintf(idx, STR("Usage      : %s%s %s\n"), dcc_prefix, cmds[o].name, cmds[o].usage ? cmds[o].usage : "");
      dprintf(idx, STR("Description: %s\n"), cmds[o].desc ? cmds[o].desc : "None");
      break;
    }
  }
  */
  
  if (showall) {
    qsort(cmds, o, sizeof(mycmds), (int (*)()) &my_cmp);
    end = 0;
    buf[0] = '\0';
    while (!done) {
      flag = newsplit(&fcats);
      if (!flag[0]) 
	done = 1;

      i = 0;
      first = 1;
      for (n = 0; n < o ; n++) { /* loop each command */
        if (!flagrec_ok(&cmds[n].flags, &fr))
          continue;
        flg[0] = '\0';
        build_flags(flg, &(cmds[n].flags), NULL);
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
        
          sprintf(buf, "%s%-14.14s", buf[0] ? buf : "", cmds[n].name);
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
    dprintf(idx, "%s\n", buf[0] ? buf : "");
  }

  if (showall) {
    dprintf(idx, STR("End of list. For individual command help, type: %shelp <command>\n"), dcc_prefix);
    dprintf(idx, STR("If you have flags on a channel, type %sconsole #chan to see more commands.\n"), dcc_prefix);
  } else if (!fnd) {
    dprintf(idx, STR("No help for nonexistant command '%s'.\n"), par);
  }
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
    if (!egg_strcasecmp(par, botnetnick))
      tell_who(u, idx, dcc[idx].u.chat->channel);
    else {
      i = nextbot(par);
      if (i < 0) {
	dprintf(idx, STR("That bot isn't connected.\n"));
      } else if (dcc[idx].u.chat->channel > 99999)
	dprintf(idx, STR("You are on a local channel.\n"));
      else {
	char s[40];

	simple_sprintf(s, "%d:%s@%s", dcc[idx].sock,
		       dcc[idx].nick, botnetnick);
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
  char *s, *s1, *chname;

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

  for (u=userlist;u;u=u->next) {
    if ((u->flags & USER_BOT) && (u->flags & USER_CHANHUB)) {
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
  cnt=0;

#ifdef HUB
  for (u=userlist;u;u=u->next) {
    if (!(u->flags & USER_BOT) && (u->flags & USER_ADMIN)) {
      if (cnt)
        dprintf(idx, ", ");
      else
        dprintf(idx, STR("Admins  : "));
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
  cnt=0;


  for (u=userlist;u;u=u->next) {
    if (!(u->flags & (USER_BOT | USER_ADMIN)) && (u->flags & USER_OWNER)) {
      if (cnt)
        dprintf(idx, ", ");
      else
        dprintf(idx, STR("Owners  : "));
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
  cnt=0;

  for (u=userlist;u;u=u->next) {
    if (!(u->flags & (USER_BOT | USER_OWNER)) && (u->flags & USER_MASTER)) {
      if (cnt)
        dprintf(idx, ", ");
      else
        dprintf(idx, STR("Masters : "));
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
  cnt=0;
#endif /* HUB */

  for (u=userlist;u;u=u->next) {
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
        cnt=0;
      }
    }
  }
  if (cnt)
    dprintf(idx, "\n");
  cnt=0;

  for (u=userlist;u;u=u->next) {
    if (!(u->flags & (USER_BOT | USER_OP))) {
      if (cnt)
        dprintf(idx, ", ");
      else
        dprintf(idx, STR("Users   : "));
      dprintf(idx, u->handle);
      cnt++;
      tt++;
      if (cnt==15) {
        dprintf(idx, "\n");
        cnt=0;
      }
    }
  }
  if (cnt)
    dprintf(idx, "\n");
  cnt=0;
  dprintf(idx, STR("Total users: %d\n"), tt);
}

static void cmd_channels(struct userrec *u, int idx, char *par) {
  putlog(LOG_CMDS, "*", STR("#%s# channels %s"), dcc[idx].nick, par);
  if (par[0] && (u->flags & USER_MASTER)) {
    struct userrec *user;
    user = get_user_by_handle(userlist, par);
    if (user)
      show_channels(idx, par);
    else 
      dprintf(idx, STR("There is no user by that name.\n"));
  } else
      show_channels(idx, NULL);

  if ((u->flags & USER_MASTER))
    dprintf(idx, STR("You can also %schannels <nickname>\n"), dcc_prefix);
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
    tell_mem_status_dcc(idx);
    dprintf(idx, "\n");
    tell_settings(idx);
    do_module_report(idx, 1, NULL);
  } else {
    putlog(LOG_CMDS, "*", STR("#%s# status"), dcc[idx].nick);
    tell_verbose_status(idx);
    tell_mem_status_dcc(idx);
    do_module_report(idx, 0, NULL);
  }
}

#ifdef HUB
static void cmd_dccstat(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# dccstat"), dcc[idx].nick);
  tell_dcc(idx);
}
#endif /* HUB */

static void cmd_boot(struct userrec *u, int idx, char *par)
{
  int i, ok = 0;
  char *who;
  struct userrec *u2;

  if (!par[0]) {
    dprintf(idx, STR("Usage: boot nick[@bot]\n"));
    return;
  }
  who = newsplit(&par);
  if (strchr(who, '@') != NULL) {
    char whonick[HANDLEN + 1];

    splitcn(whonick, who, '@', HANDLEN + 1);
    if (!egg_strcasecmp(who, botnetnick)) {
      cmd_boot(u, idx, whonick);
      return;
    }
    if (remote_boots > 0) {
      i = nextbot(who);
      if (i < 0) {
        dprintf(idx, STR("No such bot connected.\n"));
        return;
      }
      botnet_send_reject(i, dcc[idx].nick, botnetnick, whonick,
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
  char *nick, s[2], s1[512];
  int dest = 0, i, ok = 0, pls, md;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  module_entry *me;
  struct chanset_t *chan;

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

    if (private(fr, findchan_by_dname(nick), PRIV_OP)) {
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
	  md &= ~(LOG_RAW | LOG_SRVOUT | LOG_BOTNET | LOG_BOTSHARE);
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
  /* New style autosave -- drummer,07/25/1999*/
  if ((me = module_find("console", 0, 0))) {
    Function *func = me->funcs;
    (func[CONSOLE_DOSTORE]) (dest);
  }
}

#ifdef HUB
static void cmd_chhandle(struct userrec *u, int idx, char *par)
{
  char hand[HANDLEN + 1], newhand[HANDLEN + 1];
  int i, atr = u ? u->flags : 0, atr2;
  struct userrec *u2;

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
    else if (!egg_strcasecmp(newhand, botnetnick) && (!(atr2 & USER_BOT) ||
             nextbot(hand) != -1))
      dprintf(idx, STR("Hey! That's MY name!\n"));
    else if (change_handle(u2, newhand)) {
      putlog(LOG_CMDS, "*", STR("#%s# chhandle %s %s"), dcc[idx].nick,
            hand, newhand);
      dprintf(idx, STR("Changed.\n"));
    } else
      dprintf(idx, STR("Failed.\n"));
  }
}
#endif /* HUB */

static void cmd_handle(struct userrec *u, int idx, char *par)
{
  char oldhandle[HANDLEN + 1], newhandle[HANDLEN + 1];
  int i;

  strncpyz(newhandle, newsplit(&par), sizeof newhandle);

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
  } else if (!egg_strcasecmp(newhandle, botnetnick)) {
    dprintf(idx, STR("Hey!  That's MY name!\n"));
  } else {
    strncpyz(oldhandle, dcc[idx].nick, sizeof oldhandle);
    if (change_handle(u, newhandle)) {
      putlog(LOG_CMDS, "*", STR("#%s# handle %s"), oldhandle, newhandle);
      dprintf(idx, STR("Okay, changed.\n"));
    } else
      dprintf(idx, STR("Failed.\n"));
  }
}

#ifdef HUB
static void cmd_chpass(struct userrec *u, int idx, char *par)
{
  char *handle, *new, pass[16];
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
      }
    }
  }
}

static void cmd_chsecpass(struct userrec *u, int idx, char *par)
{
  char *handle, *new, pass[17];
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
      dprintf(idx, STR("You can't change a bot owner's password.\n"));
    else if (isowner(handle) && egg_strcasecmp(dcc[idx].nick, handle))
      dprintf(idx, STR("You can't change a permanent bot owner's password.\n"));
    else if (!par[0]) {
      putlog(LOG_CMDS, "*", STR("#%s# chsecpass %s [nothing]"), dcc[idx].nick,
	     handle);
      set_user(&USERENTRY_SECPASS, u, NULL);
      dprintf(idx, STR("Removed secpass.\n"));
    } else {

      l = strlen(new = newsplit(&par));
      if (l > 16)
	new[16] = 0;
      if (!strcmp(new, "rand")) {
        make_rand_str(pass, 16);
      } else {
        if (strlen(new) < 6) {
          dprintf(idx, STR("Please use at least 6 characters.\n"));
          return;
        } else {
          sprintf(pass, "%s", new);
        }
      }
      if (strlen(pass) > 16)
        pass[16] = 0;
      set_user(&USERENTRY_SECPASS, u, pass);
      putlog(LOG_CMDS, "*", STR("#%s# chsecpass %s [something]"), dcc[idx].nick,
            handle);
      dprintf(idx, STR("Secpass for '%s' changed to: %s\n"), handle, pass);
    }
  }
}

static void cmd_botcmd(struct userrec *u, int idx, char *par)
{
  tand_t *tbot;
  int cnt = 0;
  char *botm = NULL, *cmd = NULL;
  
  botm = newsplit(&par);
  cmd = newsplit(&par);
  if (!botm[0] || !cmd[0]) {
    dprintf(idx, STR("Usage: botcmd bot cmd params\n"));
    return;
  }

  /* the rest of the cmd will be logged remotely */
  putlog(LOG_CMDS, "*", STR("#%s# botcmd %s %s ..."), dcc[idx].nick, botm, cmd);	
  if (!strcmp(botm, "*") && (!strcmp(botm, "di") || !strcmp(botm, "die"))) {
    dprintf(idx, STR("Not a good idea.\n"));
    return;
  }
  for (tbot = tandbot; tbot; tbot = tbot->next) {
    if (wild_match(botm, tbot->bot)) {
      cnt++;
      send_remote_simul(idx, tbot->bot, cmd, par ? par : "");
    }
  }
   
  if (!cnt) {
    dprintf(idx, STR("No bot matching '%s' linked\n"), botm);
    return;
  }
}

static void cmd_hublevel(struct userrec *u, int idx, char *par)
{
  char *handle, *level;
  struct bot_addr *bi, *obi;
  struct userrec *u1;

  putlog(LOG_CMDS, "*", STR("#%s# hublevel %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: hublevel <botname> <level>\n"));
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
  bi = user_malloc(sizeof(struct bot_addr));

  bi->uplink = user_malloc(strlen(obi->uplink) + 1);
  strcpy(bi->uplink, obi->uplink);
  bi->address = user_malloc(strlen(obi->address) + 1);
  strcpy(bi->address, obi->address);
  bi->telnet_port = obi->telnet_port;
  bi->relay_port = obi->relay_port;
  bi->hublevel = atoi(level);
  set_user(&USERENTRY_BOTADDR, u1, bi);
  write_userfile(idx);
}

static void cmd_uplink(struct userrec *u, int idx, char *par)
{
  char *handle, *uplink;
  struct bot_addr *bi, *obi;
  struct userrec *u1;

  putlog(LOG_CMDS, "*", STR("#%s# uplink %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: uplink <botname> [<uplink>]\n"));
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
  dprintf(idx, STR("Changed bot's uplink.\n"));
  obi = get_user(&USERENTRY_BOTADDR, u1);
  bi = user_malloc(sizeof(struct bot_addr));

  bi->uplink = user_malloc(strlen(uplink) + 1);
  strcpy(bi->uplink, uplink);
  bi->address = user_malloc(strlen(obi->address) + 1);
  strcpy(bi->address, obi->address);
  bi->telnet_port = obi->telnet_port;
  bi->relay_port = obi->relay_port;
  bi->hublevel = obi->hublevel;
  set_user(&USERENTRY_BOTADDR, u1, bi);
}


static void cmd_chaddr(struct userrec *u, int idx, char *par)
{
  int telnet_port = 3333, relay_port = 3333;
#ifdef USE_IPV6
  char *handle, *addr, *p, *q, *r;
#else
  char *handle, *addr, *p, *q;
#endif /* USE_IPV6 */
  struct bot_addr *bi, *obi;
  struct userrec *u1;

  handle = newsplit(&par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: chaddr <botname> <address[:telnet-port[/relay-port]]>\n"));
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

  bi = user_malloc(sizeof(struct bot_addr));

  bi->uplink = user_malloc(strlen(obi->uplink) + 1);
  strcpy(bi->uplink, obi->uplink);
  bi->hublevel = obi->hublevel;

  q = strchr(addr, ':');
  if (!q) {
    bi->address = user_malloc(strlen(addr) + 1);
    strcpy(bi->address, addr);
    bi->telnet_port = telnet_port;
    bi->relay_port = relay_port;
  } else {
#ifdef USE_IPV6
    r = strchr(addr, '[');
    if (r) { /* ipv6 notation [3ffe:80c0:225::] */
      *addr++;
      r = strchr(addr, ']');
      bi->address = user_malloc(r - addr + 1);
      strncpyz(bi->address, addr, r - addr + 1);
      addr = r;
      *addr++;
    } else {
      bi->address = user_malloc(q - addr + 1);
      strncpyz(bi->address, addr, q - addr + 1);
    }
    q = strchr(addr, ':');
    if (q) {
      p = q + 1;
      bi->telnet_port = atoi(p);
      q = strchr(p, '/');
      if (!q) {
        bi->relay_port = telnet_port;
      } else {
        bi->relay_port = atoi(q + 1);
      }
    }
#else
    bi->address = user_malloc(q - addr + 1);
    strncpyz(bi->address, addr, q - addr + 1);
    p = q + 1;
    bi->telnet_port = atoi(p);
    q = strchr(p, '/');
    if (!q)
      bi->relay_port = bi->telnet_port;
    else
      bi->relay_port = atoi(q + 1);
#endif /* USE_IPV6 */
  }
  set_user(&USERENTRY_BOTADDR, u1, bi);
}
#endif /* HUB */

static void cmd_comment(struct userrec *u, int idx, char *par)
{
  char *handle;
  struct userrec *u1;
  handle = newsplit(&par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: comment <handle> <newcomment>\n"));
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
}

static void cmd_randstring(struct userrec *u, int idx, char *par)
{
  int len;
  char *rand;

  if (!par[0])
    return;

  putlog(LOG_CMDS, "*", STR("#%s# randstring %s"), dcc[idx].nick, par);

  len = atoi(par);
  rand = nmalloc(len + 1);
  make_rand_str(rand, len);
  dprintf(idx, STR("string: %s\n"), rand);
  nfree(rand);
}

static void cmd_restart(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# restart"), dcc[idx].nick);
  dprintf(idx, STR("To restart just '%die'\n"), dcc_prefix);
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
  char s1[1024], s2[1024];

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
  debug_mem_to_dcc(idx);
}

static void cmd_simul(struct userrec *u, int idx, char *par)
{
  char *nick;
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
  char *s;
  int i;

  if (!par[0]) {
    dprintf(idx, STR("Usage: link [some-bot] <new-bot>\n"));
    return;
  }
  putlog(LOG_CMDS, "*", STR("#%s# link %s"), dcc[idx].nick, par);
  s = newsplit(&par);
  if (!par[0] || !egg_strcasecmp(par, botnetnick))
    botlink(dcc[idx].nick, idx, s);
  else {
    char x[40];

    i = nextbot(s);
    if (i < 0) {
      dprintf(idx, STR("No such bot online.\n"));
      return;
    }
    simple_sprintf(x, "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, botnetnick);
    botnet_send_link(i, x, s, par);
  }
}
#endif /* HUB */

static void cmd_unlink(struct userrec *u, int idx, char *par)
{
  int i;
  char *bot;

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
    char x[40];

    simple_sprintf(x, "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, botnetnick);
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
  putlog(LOG_CMDS, "*", STR("#%s# save"), dcc[idx].nick);
  dprintf(idx, STR("Saving user file...\n"));
  write_userfile(-1);
}

static void cmd_backup(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# backup"), dcc[idx].nick);
  dprintf(idx, STR("Backing up the channel & user files...\n"));
  call_hook(HOOK_BACKUP);
}

static void cmd_trace(struct userrec *u, int idx, char *par)
{
  int i;
  char x[NOTENAMELEN + 11], y[11];

  if (!par[0]) {
    dprintf(idx, STR("Usage: trace <botname>\n"));
    return;
  }
  if (!egg_strcasecmp(par, botnetnick)) {
    dprintf(idx, STR("That's me!  Hiya! :)\n"));
    return;
  }
  i = nextbot(par);
  if (i < 0) {
    dprintf(idx, STR("Unreachable bot.\n"));
    return;
  }
  putlog(LOG_CMDS, "*", STR("#%s# trace %s"), dcc[idx].nick, par);
  simple_sprintf(x, "%d:%s@%s", dcc[idx].sock, dcc[idx].nick, botnetnick);
  simple_sprintf(y, ":%d", now);
  botnet_send_trace(i, x, par, y);
}

static void cmd_binds(struct userrec *u, int idx, char *par)
{
  putlog(LOG_CMDS, "*", STR("#%s# binds %s"), dcc[idx].nick, par);
  tell_binds(idx, par);
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
        do_boot(i, botnetnick, STR("No hub access.\n\n"));
      }     
#else /* !HUB */
      if (ischanhub() && !(u->flags & (USER_CHUBA))) {
        /* no chanhub access, drop them. */
        dprintf(i, STR("-+- POOF! -+-\n"));
        dprintf(i, STR("You no longer have chathub access.\n\n"));
        do_boot(i, botnetnick, STR("No chathub access.\n\n"));
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
  struct chanset_t *chan;

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
  char *hand, *arg = NULL, *tmpchg = NULL, *chg = NULL, work[1024];
  struct chanset_t *chan = NULL;
  struct userrec *u2;
  struct flag_record pls = {0, 0, 0, 0, 0, 0},
  		     mns = {0, 0, 0, 0, 0, 0},
		     user = {0, 0, 0, 0, 0, 0},
		     ouser = {0, 0, 0, 0, 0, 0};
  module_entry *me;
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
      tmpchg = nmalloc(strlen(chg) + 2);
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
      nfree(tmpchg);
    return;
  }
    if (chan && private(user, chan, PRIV_OP)) {
      dprintf(idx, STR("You do not have access to change flags for %s\n"), chan->dname);
      if (tmpchg)
        nfree(tmpchg);
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
    char tmp[100];
    putlog(LOG_CMDS, "*", STR("#%s# (%s) chattr %s %s"), dcc[idx].nick, chan ? chan->dname : "*", hand, chg ? chg : "");
    snprintf(tmp, sizeof tmp, "chattr %s", chg);
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
  if (chg && (me = module_find("irc", 0, 0))) {
    Function *func = me->funcs;

    (func[IRC_CHECK_THIS_USER]) (hand, 0, NULL);
  }
  if (tmpchg)
    nfree(tmpchg);
}

static void cmd_chat(struct userrec *u, int idx, char *par)
{
  char *arg;
  int newchan, oldchan;
  module_entry *me;

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
      check_tcl_chpt(botnetnick, dcc[idx].nick, dcc[idx].sock,
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
	  Tcl_SetVar(interp, "chan", arg, 0);
	  if ((Tcl_VarEval(interp, "assoc ", "$chan", NULL) == TCL_OK) &&
	      interp->result[0])
	    newchan = atoi(interp->result);
	  else
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
	  Tcl_SetVar(interp, "chan", arg, 0);
	  if ((Tcl_VarEval(interp, "assoc ", "$chan", NULL) == TCL_OK) &&
	      interp->result[0])
	    newchan = atoi(interp->result);
	  else
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
	check_tcl_chpt(botnetnick, dcc[idx].nick, dcc[idx].sock, oldchan);
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
      check_tcl_chjn(botnetnick, dcc[idx].nick, newchan, geticon(idx),
		     dcc[idx].sock, dcc[idx].host);
      if (newchan < GLOBAL_CHANS)
	botnet_send_join_idx(idx, oldchan);
      else if (oldchan < GLOBAL_CHANS)
	botnet_send_part_idx(idx, "");
    }
  }
  /* New style autosave here too -- rtc, 09/28/1999*/
  if ((me = module_find("console", 0, 0))) {
    Function *func = me->funcs;
    (func[CONSOLE_DOSTORE]) (idx);
  }
}

int exec_str(int idx, char *cmd) {
  char *out, *err, *p, *np;
  if (shell_exec(cmd, NULL, &out, &err)) {
    if (out) {
      dprintf(idx, STR("Result:\n"));
      p=out;
      while (p && p[0]) {
        np=strchr(p, '\n');
        if (np)
          *np++=0;
        dprintf(idx, STR("%s\n"), p);
        p=np;
      }
      dprintf(idx, "\n");
      nfree(out);
    }
    if (err) {
      dprintf(idx, STR("Errors:\n"));
      p=err;
      while (p && p[0]) {
        np=strchr(p, '\n');
        if (np)
          *np++=0;
        dprintf(idx, STR("%s\n"), p);
        p=np;
      }
      dprintf(idx, "\n");
      nfree(err);
    }
    return 1;
  }
  return 0;
}

static void cmd_exec(struct userrec *u, int idx, char *par) {
  putlog(LOG_CMDS, "*", STR("#%s# exec %s"), dcc[idx].nick, par);
#ifdef LEAF
  if (!isowner(u->handle)) {
    putlog(LOG_WARN, "*", STR("%s attempted 'exec' %s"), dcc[idx].nick, par);
    dprintf(idx, STR("exec is only available to permanent owners on leaf bots\n"));
    return;
  }
#endif /* LEAF */
  if (exec_str(idx, par))
    dprintf(idx, STR("Exec completed\n"));
  else
    dprintf(idx, STR("Exec failed\n"));
}

static void cmd_w(struct userrec *u, int idx, char *par) {
  putlog(LOG_CMDS, "*", STR("#%s# w"), dcc[idx].nick);
  if (!exec_str(idx, "w"))
    dprintf(idx, STR("Exec failed\n"));
}

static void cmd_ps(struct userrec *u, int idx, char *par) {
  char * buf;
  putlog(LOG_CMDS, "*", STR("#%s# ps %s"), dcc[idx].nick, par);
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    putlog(LOG_WARN, "*", STR("%s attempted 'ps' with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, STR("No."));
    return;
  }
  buf=nmalloc(strlen(par)+4);
  sprintf(buf, STR("ps %s"), par);
  if (!exec_str(idx, buf))
    dprintf(idx, STR("Exec failed\n"));
  nfree(buf);
}

static void cmd_last(struct userrec *u, int idx, char *par) {
  char user[20], buf[30];
  struct passwd *pw;

  putlog(LOG_CMDS, "*", STR("#%s# last %s"), dcc[idx].nick, par);
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    putlog(LOG_WARN, "*", STR("%s attempted 'last' with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, STR("No."));
    return;
  }
  if (par[0]) {
    strncpyz(user, par, sizeof(user));
  } else {
    pw = getpwuid(geteuid());
    if (!pw) 
      return;
    strncpyz(user, pw->pw_name, sizeof(user));
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
  module_entry *me;

  if (!par[0]) {
    dprintf(idx, STR("Echo is currently %s.\n"), dcc[idx].status & STAT_ECHO ?
	    "on" : "off");
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
  /* New style autosave here too -- rtc, 09/28/1999*/
  if ((me = module_find("console", 0, 0))) {
    Function *func = me->funcs;
    (func[CONSOLE_DOSTORE]) (idx);
  }
}

static void cmd_color(struct userrec *u, int idx, char *par)
{
  module_entry *me;

  char *type, *of;
  putlog(LOG_CMDS, "*", STR("#%s# color %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: color <on/off> <mIRC/ANSI>\n"));
    if (dcc[idx].status & STAT_COLOR) 
      dprintf(idx, STR("Color is currently on (%s).\n"), dcc[idx].status & STAT_COLORM ? "mIRC" : "ANSI");
    else
      dprintf(idx, STR("Color is currently off.\n"));
    return;
  }
  of = newsplit(&par);
  type = newsplit(&par);

  if (!egg_strcasecmp(of, "on")) {
    if (!type) {
      dprintf(idx, STR("Usage: color <on/off> <mIRC/ANSI>\n"));
      return;
    }
    if (!egg_strcasecmp(type, "mirc")) {
      dcc[idx].status &= ~STAT_COLORA;
      dcc[idx].status |= (STAT_COLOR | STAT_COLORM);
      dprintf(idx, STR("Color turned on (mIRC).\n"));
    } else if (!egg_strcasecmp(type, "ansi")) {
      dcc[idx].status &= ~STAT_COLORM;
      dcc[idx].status |= (STAT_COLOR | STAT_COLORA);
      dprintf(idx, STR("Color turned on (ANSI).\n"));
    } else {
      return;
    }
  } else if (!egg_strcasecmp(of, "off")) {
    dcc[idx].status &= ~(STAT_COLOR | STAT_COLORM | STAT_COLORA);
    dprintf(idx, STR("Color turned off.\n"));
  } else {
    return;
  }

  /* New style autosave here too -- rtc, 09/28/1999*/
  if ((me = module_find("console", 0, 0))) {
    Function *func = me->funcs;
    (func[CONSOLE_DOSTORE]) (idx);
  }
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
  static char s[20];
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
  static char s[161];
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
  char *nick, *changes, *c, s[2];
  int dest = 0, i, pls, md, ok = 0;
  module_entry *me;

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
  /* New style autosave here too -- rtc, 09/28/1999*/
  if ((me = module_find("console", 0, 0))) {
    Function *func = me->funcs;
    (func[CONSOLE_DOSTORE]) (dest);
  }
}

static void cmd_su(struct userrec *u, int idx, char *par)
{
  int atr = u ? u->flags : 0;
  int ok;
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
	  nfree(dcc[idx].u.chat->away);
        dcc[idx].u.chat->away = get_data_ptr(strlen(dcc[idx].nick) + 1);
	strcpy(dcc[idx].u.chat->away, dcc[idx].nick);
        dcc[idx].u.chat->su_nick = get_data_ptr(strlen(dcc[idx].nick) + 1);
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
        dcc[idx].u.chat->su_nick = get_data_ptr(strlen(dcc[idx].nick) + 1);
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
  module_entry *me;

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
  /* New style autosave here too -- rtc, 09/28/1999*/
  if ((me = module_find("console", 0, 0))) {
    Function *func = me->funcs;
    (func[CONSOLE_DOSTORE]) (idx);
  }
}

/* Evaluate a Tcl command, send output to a dcc user.
 */
#ifdef S_TCLCMDS
static void cmd_tcl(struct userrec *u, int idx, char *msg)
{
  int code;
#ifdef S_PERMONLY
  if (!(isowner(dcc[idx].nick)) && (must_be_owner)) {
    dprintf(idx, STR("What?  You need '%shelp'\n"), dcc_prefix);
    return;
  }
#endif /* S_PERMONLY */
  putlog(LOG_CMDS, "*", STR("#%s# tcl %s"), dcc[idx].nick, msg);
  debug1(STR("tcl: evaluate (.tcl): %s"), msg);
  code = Tcl_GlobalEval(interp, msg);
  if (code == TCL_OK)
    dumplots(idx, STR("Tcl: "), interp->result);
  else
    dumplots(idx, STR("Tcl error: "), interp->result);
}
#endif /* S_TCLCMDS */

#ifdef HUB
#ifdef S_TCLCMDS
static void cmd_nettcl(struct userrec *u, int idx, char *msg)
{
  int code;
  char buf[2000];
#ifdef S_PERMONLY
  if (!(isowner(dcc[idx].nick)) && (must_be_owner)) {
    dprintf(idx, STR("What?  You need '%shelp'\n"), dcc_prefix);
    return;
  }
#endif /* S_PERMONLY */
  putlog(LOG_CMDS, "*", STR("#%s# nettcl %s"), dcc[idx].nick, msg);
  egg_snprintf(buf, sizeof buf, "mt %d %s", idx, msg);
  botnet_send_zapf_broad(-1, botnetnick, NULL, buf);

  debug1(STR("tcl: evaluate (.tcl): %s"), msg);
  code = Tcl_GlobalEval(interp, msg);
  if (code == TCL_OK)
    dumplots(idx, STR("Tcl: "), interp->result);
  else
    dumplots(idx, STR("Tcl error: "), interp->result);
}
#endif /* S_TCLCMDS */

static void cmd_newleaf(struct userrec *u, int idx, char *par)
{
  char *handle,
   *host;
  struct userrec *u1;
  struct bot_addr *bi;

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
      userlist = adduser(userlist, handle, STR("none"), "-", USER_BOT | USER_OP);
      u1 = get_user_by_handle(userlist, handle);
      bi = user_malloc(sizeof(struct bot_addr));

      bi->uplink = user_malloc(strlen(botnetnick) + 1); 
/*      strcpy(bi->uplink, botnetnick); */
      strcpy(bi->uplink, "");

      bi->address = user_malloc(1);
      bi->address[0] = 0;
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
      dprintf(idx, STR("Added new leaf: %s\n"), handle);
    }
  }
}

/* Perform a 'set' command
 */
static void cmd_set(struct userrec *u, int idx, char *msg)
{
  int code;
  char s[512];
#ifdef S_PERMONLY
  if (!(isowner(dcc[idx].nick)) && (must_be_owner)) {
    dprintf(idx, STR("What?  You need '%shelp'\n"), dcc_prefix);
    return;
  }
#endif /* S_PERMONLY */
  putlog(LOG_CMDS, "*", STR("#%s# set %s"), dcc[idx].nick, msg);
  strcpy(s, "set ");
  if (!msg[0]) {
    strcpy(s, "info globals");
    Tcl_Eval(interp, s);
    dumplots(idx, STR("Global vars: "), interp->result);
    return;
  }
  strcpy(s + 4, msg);
  code = Tcl_Eval(interp, s);
  if (code == TCL_OK) {
    if (!strchr(msg, ' '))
      dumplots(idx, STR("Currently: "), interp->result);
    else
      dprintf(idx, STR("Ok, set.\n"));
  } else
    dprintf(idx, STR("Error: %s\n"), interp->result);
}
#endif /* HUB */

static void cmd_pls_ignore(struct userrec *u, int idx, char *par)
{
  char			*who;
  char			 s[UHOSTLEN];
  unsigned long int	 expire_time = 0;

  if (!par[0]) {
    dprintf(idx, STR("Usage: +ignore <hostmask> [%%<XdXhXm>] [comment]\n"));
    return;
  }

  who = newsplit(&par);
  if (par[0] == '%') {
    char		*p, *p_expire;
    unsigned long int	 expire_foo;

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
  }
}

static void cmd_mns_ignore(struct userrec *u, int idx, char *par)
{
  char buf[UHOSTLEN];

  if (!par[0]) {
    dprintf(idx, STR("Usage: -ignore <hostmask | ignore #>\n"));
    return;
  }
  strncpyz(buf, par, sizeof buf);
  if (delignore(buf)) {
    putlog(LOG_CMDS, "*", STR("#%s# -ignore %s"), dcc[idx].nick, buf);
    dprintf(idx, STR("No longer ignoring: %s\n"), buf);
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
  char *handle, *host;
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
  else if (!egg_strcasecmp(handle, botnetnick))
    dprintf(idx, STR("Hey! That's MY name!\n"));
  else {
    struct userrec *u2;
    char tmp[50], s[16], s2[17];
    userlist = adduser(userlist, handle, host, "-", USER_DEFAULT);
    u2 = get_user_by_handle(userlist, handle);
    sprintf(tmp, STR("%lu %s"), time(NULL), u->handle);
    set_user(&USERENTRY_ADDED, u2, tmp);
    dprintf(idx, STR("Added %s (%s) with no flags.\n"), handle, host);
    while (par[0]) {
      host=newsplit(&par);
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
  char *handle;
  struct userrec *u2;
  module_entry *me;
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
  if ((me = module_find("irc", 0, 0))) {
    Function *func = me->funcs;

   (func[IRC_CHECK_THIS_USER]) (handle, 1, NULL);
  }
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
  char *handle, *host;
  struct userrec *u2;
  struct list_type *q;
  struct flag_record fr2 = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0},
                     fr  = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0};
  module_entry *me;

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
  if ((me = module_find("irc", 0, 0))) {
    Function *func = me->funcs;

   (func[IRC_CHECK_THIS_USER]) (handle, 0, NULL);
  }
}

static void cmd_mns_host(struct userrec *u, int idx, char *par)
{
  char *handle, *host;
  struct userrec *u2;
  struct flag_record fr2 = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0},
                     fr  = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0};
  module_entry *me;

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
    if ((me = module_find("irc", 0, 0))) {
      Function *func = me->funcs;

     (func[IRC_CHECK_THIS_USER]) (handle, 2, host);
    }
  } else
    dprintf(idx, STR("Failed.\n"));
}

/* netserver */

static void cmd_netserver(struct userrec * u, int idx, char * par) {
  putlog(LOG_CMDS, "*", STR("#%s# netserver"), dcc[idx].nick);
  botnet_send_cmd_broad(-1, botnetnick, u->handle, idx, STR("cursrv"));
}

static void cmd_botserver(struct userrec * u, int idx, char * par) {
  putlog(LOG_CMDS, "*", STR("#%s# botserver %s"), dcc[idx].nick, par);
  if (!par || !par[0]) {
    dprintf(idx, STR("Usage: botserver <botname>\n"));
    return;
  }
  if (nextbot(par)<0) {
    dprintf(idx, STR("%s isn't a linked bot\n"), par);
  }
  botnet_send_cmd(botnetnick, par, u->handle, idx, STR("cursrv"));
}


void rcmd_cursrv(char * fbot, char * fhand, char * fidx) {
#ifdef LEAF
  char tmp[2048], cursrvname[500];
  int server_online = 0;
  module_entry *me;

  if ((me = module_find("server", 0, 0))) {
    Function *func = me->funcs;
    server_online = (*(int *)(func[25]));
    sprintf(cursrvname, "%s", ((char *)(func[41])));
  }
  if (server_online)
    sprintf(tmp, STR("Currently: %s"), cursrvname);
  else
    sprintf(tmp, STR("Currently: none"));
  botnet_send_cmdreply(botnetnick, fbot, fhand, fidx, tmp);
#endif
}

/* netversion */
static void cmd_netversion(struct userrec * u, int idx, char * par) {
  putlog(LOG_CMDS, "*", STR("#%s# netversion"), dcc[idx].nick);
  botnet_send_cmd_broad(-1, botnetnick, u->handle, idx, STR("ver"));
}

static void cmd_botversion(struct userrec * u, int idx, char * par) {
  putlog(LOG_CMDS, "*", STR("#%s# botversion %s"), dcc[idx].nick, par);
  if (!par || !par[0]) {
    dprintf(idx, STR("Usage: botversion <botname>\n"));
    return;
  }
  if (nextbot(par)<0) {
    dprintf(idx, STR("%s isn't a linked bot\n"), par);
  }
  botnet_send_cmd(botnetnick, par, u->handle, idx, STR("ver"));
}

void rcmd_ver(char * fbot, char * fhand, char * fidx) {
  char tmp[2048];
#ifdef HAVE_UNAME
  struct utsname un;
#endif /* HAVE_UNAME */
  sprintf(tmp, STR("%s "), ver);
#ifdef HAVE_UNAME
  if (uname(&un) < 0) {
#endif /* HAVE_UNAME */
    strcat(tmp, STR("(unknown OS)"));
#ifdef HAVE_UNAME
  } else {
    sprintf(tmp + strlen(tmp), STR("%s %s (%s)"), un.sysname, un.release, un.machine);
  }
#endif /* HAVE_UNAME */
  botnet_send_cmdreply(botnetnick, fbot, fhand, fidx, tmp);
}


/* netnick, botnick */
static void cmd_netnick (struct userrec *u, int idx, char *par) {
  putlog(LOG_CMDS, "*", STR("#%s# netnick"), dcc[idx].nick);
  botnet_send_cmd_broad(-1, botnetnick, u->handle, idx, STR("curnick"));
}

static void cmd_botnick(struct userrec * u, int idx, char * par) {
  putlog(LOG_CMDS, "*", STR("#%s# botnick %s"), dcc[idx].nick, par);
  if (!par || !par[0]) {
    dprintf(idx, STR("Usage: botnick <botname>\n"));
    return;
  }
  if (nextbot(par)<0) {
    dprintf(idx, STR("%s isn't a linked bot\n"), par);
  }
  botnet_send_cmd(botnetnick, par, u->handle, idx, STR("curnick"));
}

void rcmd_curnick(char * fbot, char * fhand, char * fidx) {
#ifdef LEAF
  char tmp[1024];
  int server_online = 0;
  module_entry *me;
  if ((me = module_find("server", 0, 0))) {
    Function *func = me->funcs;
    server_online = (*(int *)(func[25]));
  }
  tmp[0] = 0;
  if (strncmp(botname, origbotname, strlen(botname)))
    sprintf(tmp, STR("Want: %s, "), origbotname);
  if (server_online)
    sprintf(tmp, STR("%sCurrently: %s "), tmp, botname);
  else
    sprintf(tmp, STR("%s(not online)"), tmp);
  botnet_send_cmdreply(botnetnick, fbot, fhand, fidx, tmp);
#endif /* LEAF */
}

/* netmsg, botmsg */
static void cmd_botmsg(struct userrec * u, int idx, char * par) {
  char * tnick, * tbot;
  char tmp[1024];
  putlog(LOG_CMDS, "*", STR("#%s# botmsg %s"), dcc[idx].nick, par);
  tbot=newsplit(&par);
  tnick=newsplit(&par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: botmsg <botname> <nick|#channel> <message>\n"));
    return;
  }
  if (nextbot(tbot)<0) {
    dprintf(idx, STR("No such bot linked\n"));
    return;
  }
  sprintf(tmp, STR("msg %s %s"), tnick, par);
  botnet_send_cmd(botnetnick, tbot, u->handle, idx, tmp);
}

static void cmd_netmsg(struct userrec * u, int idx, char * par) {
  char * tnick;
  char tmp[1024];
  putlog(LOG_CMDS, "*", STR("#%s# netmsg %s"), dcc[idx].nick, par);
  tnick=newsplit(&par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: netmsg <nick|#channel> <message>\n"));
    return;
  }
  sprintf(tmp, STR("msg %s %s"), tnick, par);
  botnet_send_cmd_broad(-1, botnetnick, u->handle, idx, tmp);
}

void rcmd_msg(char * tobot, char * frombot, char * fromhand, char * fromidx, char * par) {
#ifdef LEAF
  char buf[1024], *nick;
  nick=newsplit(&par);
  dprintf(DP_SERVER, STR("PRIVMSG %s :%s\n"), nick, par);
  if (!strcmp(tobot, botnetnick)) {
    sprintf(buf, STR("Sent message to %s"), nick);
    botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, buf);
  }
#endif /* LEAF */
}

/* netlag */
static void cmd_netlag(struct userrec * u, int idx, char * par) {
  struct timeval tv;
  time_t tm;
  char tmp[64];
  putlog(LOG_CMDS, "*", STR("#%s# netlag"), dcc[idx].nick);
  gettimeofday(&tv, NULL);
  tm = (tv.tv_sec % 10000) * 100 + (tv.tv_usec * 100) / (1000000);
  sprintf(tmp, STR("ping %lu"), tm);
  dprintf(idx, STR("Sent ping to all linked bots\n"));
  botnet_send_cmd_broad(-1, botnetnick, u->handle, idx, tmp);
}

void rcmd_ping(char * frombot, char *fromhand, char * fromidx, char * par) {
  char tmp[64];
  sprintf(tmp, STR("pong %s"), par);
  botnet_send_cmd(botnetnick, frombot, fromhand, atoi(fromidx), tmp);
}

void rcmd_pong(char *frombot, char *fromhand, char *fromidx, char *par) {
  int i=atoi(fromidx);
  if ((i>=0) && (i<dcc_total) && (dcc[i].type==&DCC_CHAT) && (!strcmp(dcc[i].nick, fromhand))) {
    struct timeval tv;
    time_t tm;
    gettimeofday(&tv, NULL);
    tm = ((tv.tv_sec % 10000) * 100 + (tv.tv_usec * 100) / (1000000)) - atoi(par);
    dprintf(i, STR("Pong from %s: %i.%i seconds\n"), frombot, (tm / 100), (tm % 100));
  }
}

/* exec commands */
#ifdef HUB
static void cmd_netw(struct userrec * u, int idx, char * par) {
  char tmp[128];
  putlog(LOG_CMDS, "*", STR("#%s# netw"), dcc[idx].nick);
  strcpy(tmp, STR("exec w"));
  botnet_send_cmd_broad(-1, botnetnick, dcc[idx].nick, idx, tmp);
}

static void cmd_netps(struct userrec * u, int idx, char * par) {
  char buf[1024];
  putlog(LOG_CMDS, "*", STR("#%s# netps %s"), dcc[idx].nick, par);
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    putlog(LOG_WARN, "*", STR("%s attempted 'netps' with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, STR("No."));
    return;
  }
  sprintf(buf, STR("exec ps %s"), par);
  botnet_send_cmd_broad(-1, botnetnick, dcc[idx].nick, idx, buf);
}

static void cmd_netlast(struct userrec * u, int idx, char * par) {
  char buf[1024];
  putlog(LOG_CMDS, "*", STR("#%s# netlast %s"), dcc[idx].nick, par);
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    putlog(LOG_WARN, "*", STR("%s attempted 'netlast' with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, STR("No."));
    return;
  }
  sprintf(buf, STR("exec last %s"), par);
  botnet_send_cmd_broad(-1, botnetnick, dcc[idx].nick, idx, buf);
}
#endif /* HUB */

void crontab_show(struct userrec * u, int idx) {
  dprintf(idx, STR("Showing current crontab:\n"));
  if (!exec_str(idx, STR("crontab -l | grep -v \"^#\"")))
    dprintf(idx, STR("Exec failed"));
}

void crontab_del() {
  char * tmpfile, *p, buf[2048];
  tmpfile=nmalloc(strlen(binname)+100);
  strcpy(tmpfile, binname);
  if (!(p=strrchr(tmpfile, '/')))
    return;
  p++;
  strcpy(p, STR(".ctb"));
  sprintf(buf, STR("crontab -l | grep -v \"%s\" | grep -v \"^#\" | grep -v \"^\\$\" > %s"), binname, tmpfile);
  if (shell_exec(buf, NULL, NULL, NULL)) {
    sprintf(buf, STR("crontab %s"), tmpfile);
    shell_exec(buf, NULL, NULL, NULL);
  }
  unlink(tmpfile);
}

static void cmd_crontab(struct userrec *u, int idx, char *par) {
  char * code;
  int i;
  putlog(LOG_CMDS, "*", STR("#%s# crontab %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: crontab status|delete|show|new [interval]\n"));
    return;
  }
  code=newsplit(&par);
  if (!strcmp(code, STR("status"))) {
    i=crontab_exists();
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
    i=crontab_exists();
    if (!i)
      dprintf(idx, STR("No crontab\n"));
    else if (i==1)
      dprintf(idx, STR("Crontabbed\n"));
    else
      dprintf(idx, STR("Error checking crontab status\n"));
  } else if (!strcmp(code, STR("new"))) {
    i=atoi(par);
    if ((i<=0) || (i>60))
      i=10;
    crontab_create(i);
    i=crontab_exists();
    if (!i)
      dprintf(idx, STR("No crontab\n"));
    else if (i==1)
      dprintf(idx, STR("Crontabbed\n"));
    else
      dprintf(idx, STR("Error checking crontab status\n"));
  } else {
    dprintf(idx, STR("Usage: crontab status|delete|show|new [interval]\n"));
  }
}

#ifdef HUB
static void cmd_netcrontab(struct userrec * u, int idx, char * par) {
  char buf[1024], *cmd;
  putlog(LOG_CMDS, "*", STR("#%s# netcrontab %s"), dcc[idx].nick, par);
  cmd=newsplit(&par);
  if ((strcmp(cmd, STR("status")) && strcmp(cmd, STR("show")) && strcmp(cmd, STR("delete")) && strcmp(cmd, STR("new")))) {
    dprintf(idx, STR("Usage: netcrontab status|delete|show|new [interval]\n"));
    return;
  }
  egg_snprintf(buf, sizeof buf, STR("exec crontab %s %s"), cmd, par);
  botnet_send_cmd_broad(-1, botnetnick, dcc[idx].nick, idx, buf);
}
#endif /* HUB */

void rcmd_exec(char * frombot, char * fromhand, char * fromidx, char * par) {
  char * cmd, scmd[512], *out, *err;
  struct passwd *pw;

  cmd=newsplit(&par);
  scmd[0]=0;
  if (!strcmp(cmd, "w")) {
    strcpy(scmd, "w");
  } else if (!strcmp(cmd, STR("last"))) {
    char user[20];
    if (par[0]) {
      strncpyz(user, par, sizeof(user));
    } else {
      pw = getpwuid(geteuid());
      if (!pw) return;
      strncpyz(user, pw->pw_name, sizeof(user));
    }
    if (!user[0]) {
      botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, STR("Can't determine user id for process"));
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
    char * code=newsplit(&par);
    scmd[0]=0;
    if (!strcmp(code, STR("show"))) {
      strcpy(scmd, STR("crontab -l | grep -v \"^#\""));
    } else if (!strcmp(code, STR("delete"))) {
      crontab_del();
    } else if (!strcmp(code, STR("new"))) {
      int i=atoi(par);
      if ((i<=0) || (i>60))
        i=10;
      crontab_create(i);
    }
    if (!scmd[0]) {
      char s[200];
      int i;
      i=crontab_exists();
      if (!i)
        sprintf(s, STR("No crontab"));
      else if (i==1)
        sprintf(s, STR("Crontabbed"));
      else
        sprintf(s, STR("Error checking crontab status"));
      botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, s);
    }
  }
  if (!scmd[0])
    return;
  if (shell_exec(scmd, NULL, &out, &err)) {
    if (out) {
      char * p, * np;
      botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, STR("Result:"));
      p=out;
      while (p && p[0]) {
        np=strchr(p, '\n');
        if (np)
          *np++=0;
        botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, p);
        p=np;
      }
      nfree(out);
    }
    if (err) {
      char * p, * np;
      botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, STR("Errors:"));
      p=err;
      while (p && p[0]) {
        np=strchr(p, '\n');
        if (np)
          *np++=0;
        botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, p);
        p=np;
      }
      nfree(err);
    }
  } else {
    botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, STR("exec failed"));
  }

}

static void cmd_botjump(struct userrec * u, int idx, char * par) {
  char *tbot, buf[1024];
  putlog(LOG_CMDS, "*", STR("#%s# botjump %s"), dcc[idx].nick, par);
  tbot=newsplit(&par);
  if (!tbot[0]) {
    dprintf(idx, STR("Usage: botjump <botname> [server [port [pass]]]\n"));
    return;
  }
  if (nextbot(tbot)<0) {
    dprintf(idx, STR("No such linked bot\n"));
    return;
  }
  sprintf(buf, STR("jump %s"), par);
  botnet_send_cmd(botnetnick, tbot, dcc[idx].nick, idx, buf);
}

void rcmd_jump(char * frombot, char * fromhand, char * fromidx, char * par) {
#ifdef LEAF
  char * other;
  module_entry *me;
  Function *func;
  int port, default_port = 0;

  if (!(me = module_find("server", 0, 0)) )
    return;
  func = me->funcs;

  default_port = (*(int *)(func[24]));

  if (par[0]) {
    other = newsplit(&par);
    port = atoi(newsplit(&par));
    if (!port)
      port = default_port;
    strncpyz(((char *)(func[20])), other, 120); //newserver
    (*(int *)(func[21])) = port; //newserverport
    strncpyz(((char *)(func[22])), par, 120); //newserverpass
  }
  botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, STR("Jumping..."));

  (*(int *)(func[23])) = 0; //cycle_time
  (func[SERVER_NUKESERVER]) ("jumping...");
#endif /* LEAF */
}


/* "Remotable" commands */
void gotremotecmd (char * forbot, char * frombot, char * fromhand, char * fromidx, char * cmd) {
  char * par = cmd;
  cmd=newsplit(&par);
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
    botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, STR("Unrecognized remote command"));
  }
}

void gotremotereply (char * frombot, char * tohand, char * toidx, char * ln) {
  int idx=atoi(toidx);
  if ((idx>=0) && (idx<dcc_total) && (dcc[idx].type == &DCC_CHAT) && (!strcmp(dcc[idx].nick, tohand))) {
    dprintf(idx, STR("(%s) %s\n"), frombot, ln);
  }
}

static void cmd_traffic(struct userrec *u, int idx, char *par)
{
  unsigned long itmp, itmp2;

  dprintf(idx, STR("Traffic since last restart\n"));
  dprintf(idx, "==========================\n");
  if (otraffic_irc > 0 || itraffic_irc > 0 || otraffic_irc_today > 0 ||
      itraffic_irc_today > 0) {
    dprintf(idx, "IRC:\n");
    dprintf(idx, "  out: %s", btos(otraffic_irc + otraffic_irc_today));
              dprintf(idx, " (%s today)\n", btos(otraffic_irc_today));
    dprintf(idx, "   in: %s", btos(itraffic_irc + itraffic_irc_today));
              dprintf(idx, " (%s today)\n", btos(itraffic_irc_today));
  }
  if (otraffic_bn > 0 || itraffic_bn > 0 || otraffic_bn_today > 0 ||
      itraffic_bn_today > 0) {
    dprintf(idx, STR("Botnet:\n"));
    dprintf(idx, "  out: %s", btos(otraffic_bn + otraffic_bn_today));
              dprintf(idx, " (%s today)\n", btos(otraffic_bn_today));
    dprintf(idx, "   in: %s", btos(itraffic_bn + itraffic_bn_today));
              dprintf(idx, " (%s today)\n", btos(itraffic_bn_today));
  }
  if (otraffic_dcc > 0 || itraffic_dcc > 0 || otraffic_dcc_today > 0 ||
      itraffic_dcc_today > 0) {
    dprintf(idx, STR("Partyline:\n"));
    itmp = otraffic_dcc + otraffic_dcc_today;
    itmp2 = otraffic_dcc_today;
    dprintf(idx, "  out: %s", btos(itmp));
              dprintf(idx, " (%s today)\n", btos(itmp2));
    dprintf(idx, "   in: %s", btos(itraffic_dcc + itraffic_dcc_today));
              dprintf(idx, " (%s today)\n", btos(itraffic_dcc_today));
  }
  if (otraffic_trans > 0 || itraffic_trans > 0 || otraffic_trans_today > 0 ||
      itraffic_trans_today > 0) {
    dprintf(idx, STR("Transfer.mod:\n"));
    dprintf(idx, "  out: %s", btos(otraffic_trans + otraffic_trans_today));
              dprintf(idx, " (%s today)\n", btos(otraffic_trans_today));
    dprintf(idx, "   in: %s", btos(itraffic_trans + itraffic_trans_today));
              dprintf(idx, " (%s today)\n", btos(itraffic_trans_today));
  }
  if (otraffic_unknown > 0 || otraffic_unknown_today > 0) {
    dprintf(idx, "Misc:\n");
    dprintf(idx, "  out: %s", btos(otraffic_unknown + otraffic_unknown_today));
              dprintf(idx, " (%s today)\n", btos(otraffic_unknown_today));
    dprintf(idx, "   in: %s", btos(itraffic_unknown + itraffic_unknown_today));
              dprintf(idx, " (%s today)\n", btos(itraffic_unknown_today));
  }
  dprintf(idx, "---\n");
  dprintf(idx, "Total:\n");
  itmp = otraffic_irc + otraffic_bn + otraffic_dcc + otraffic_trans
         + otraffic_unknown + otraffic_irc_today + otraffic_bn_today
         + otraffic_dcc_today + otraffic_trans_today + otraffic_unknown_today;
  itmp2 = otraffic_irc_today + otraffic_bn_today + otraffic_dcc_today
         + otraffic_trans_today + otraffic_unknown_today;
  dprintf(idx, "  out: %s", btos(itmp));
              dprintf(idx, " (%s today)\n", btos(itmp2));
  dprintf(idx, "   in: %s", btos(itraffic_irc + itraffic_bn + itraffic_dcc
	  + itraffic_trans + itraffic_unknown + itraffic_irc_today
	  + itraffic_bn_today + itraffic_dcc_today + itraffic_trans_today
	  + itraffic_unknown_today));
  dprintf(idx, " (%s today)\n", btos(itraffic_irc_today + itraffic_bn_today
          + itraffic_dcc_today + itraffic_trans_today
	  + itraffic_unknown_today));
  putlog(LOG_CMDS, "*", STR("#%s# traffic"), dcc[idx].nick);
}

static char traffictxt[20];
static char *btos(unsigned long  bytes)
{
  char unit[10];
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
  dprintf(idx, "You are %s@%s.\n", dcc[idx].nick, botnetnick);
  putlog(LOG_CMDS, "*", STR("#%s# whoami"), dcc[idx].nick);
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
  {"binds",		"a",	(Function) cmd_binds,		NULL},
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
  {"chnick",		"m",	(Function) cmd_chhandle,	NULL},
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
#ifdef HUB
  {"dccstat",		"a",	(Function) cmd_dccstat,		NULL},
#endif /* HUB */
  {"debug",		"a",	(Function) cmd_debug,		NULL},
  {"die",		"n",	(Function) cmd_die,		NULL},
  {"echo",		"",	(Function) cmd_echo,		NULL},
  {"fixcodes",		"",	(Function) cmd_fixcodes,	NULL},
  {"handle",		"",	(Function) cmd_handle,		NULL},
  {"help",		"-|-",	(Function) cmd_help,		NULL},
  {"ignores",		"m",	(Function) cmd_ignores,		NULL},
#ifdef HUB
  {"link",		"n",	(Function) cmd_link,		NULL},
#endif /* HUB */
  {"match",		"m|m",	(Function) cmd_match,		NULL},
  {"me",		"",	(Function) cmd_me,		NULL},
  {"motd",		"",	(Function) cmd_motd,		NULL},
#ifdef HUB
#ifdef S_TCLCMDS
  {"nettcl",		"a",	(Function) cmd_nettcl,		NULL},
#endif /* S_TCLCMDS */
  {"newleaf",		"n",	(Function) cmd_newleaf,		NULL},
#endif /* HUB */
  {"newpass",		"",	(Function) cmd_newpass,		NULL},
  {"secpass",		"",	(Function) cmd_secpass,		NULL},
  {"nick",		"",	(Function) cmd_handle,		NULL},
  {"page",		"",	(Function) cmd_page,		NULL},
  {"quit",		"",	(Function) NULL,		NULL},
  {"relay",		"i",	(Function) cmd_relay,		NULL},
#ifdef HUB
  {"reload",		"m|m",	(Function) cmd_reload,		NULL},
#endif /* HUB */
  {"restart",		"m",	(Function) cmd_restart,		NULL},
#ifdef HUB
  {"save",		"m|m",	(Function) cmd_save,		NULL},
  {"set",		"a",	(Function) cmd_set,		NULL},
#endif /* HUB */
  {"simul",		"a",	(Function) cmd_simul,		NULL},
  {"status",		"m|m",	(Function) cmd_status,		NULL},
  {"strip",		"",	(Function) cmd_strip,		NULL},
  {"su",		"a",	(Function) cmd_su,		NULL},
#ifdef S_TCLCMDS 
  {"tcl",		"a",	(Function) cmd_tcl,		NULL},
#endif /* S_TCLCMDS */
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
  {"botjump",           "n",    (Function) cmd_botjump,         NULL},
  {"botmsg",		"o",    (Function) cmd_botmsg,          NULL},
  {"netmsg", 		"n", 	(Function) cmd_netmsg, 		NULL},
  {"botnick", 		"m", 	(Function) cmd_botnick, 	NULL},
  {"netnick", 		"m", 	(Function) cmd_netnick, 	NULL},
#ifdef HUB
  {"netw", 		"n", 	(Function) cmd_netw, 		NULL},
  {"netps", 		"n", 	(Function) cmd_netps, 		NULL},
  {"netlast", 		"n", 	(Function) cmd_netlast, 	NULL},
#endif /* HUB */
  {"netlag", 		"m", 	(Function) cmd_netlag, 		NULL},
  {"botserver",		"m",	(Function) cmd_botserver,	NULL},
  {"netserver", 	"m", 	(Function) cmd_netserver, 	NULL},
  {"botversion", 	"o", 	(Function) cmd_botversion, 	NULL},
  {"netversion", 	"o", 	(Function) cmd_netversion, 	NULL},
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
