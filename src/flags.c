
/* 
 * flags.c -- handles:
 *   all the flag matching/conversion functions in one neat package :)
 * 
 * $Id: flags.c,v 1.9 2000/01/17 16:14:45 per Exp $
 */

/* 
 * Copyright (C) 1997  Robey Pointer
 * Copyright (C) 1999, 2000  Eggheads
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

#include "main.h"

extern int use_console_r,
  debug_output,
  noshare,
  allow_dk_cmds;
extern struct dcc_t *dcc;

int use_console_r = 0;		/* allow users to set their console +r */

int logmodes(char *s)
{
  int i;
  int res = 0;

  for (i = 0; i < strlen(s); i++)
    switch (s[i]) {
    case 'm':
    case 'M':
      res |= LOG_MSGS;
      break;
    case 'p':
    case 'P':
      res |= LOG_PUBLIC;
      break;
    case 'j':
    case 'J':
      res |= LOG_JOIN;
      break;
    case 'k':
    case 'K':
      res |= LOG_MODES;
      break;
    case 'c':
    case 'C':
      res |= LOG_CMDS;
      break;
    case 'o':
    case 'O':
      res |= LOG_MISC;
      break;
    case 'b':
    case 'B':
      res |= LOG_BOTS;
      break;
    case 'r':
    case 'R':
      res |= use_console_r ? LOG_RAW : 0;
      break;
    case 'w':
    case 'W':
      res |= LOG_WALL;
      break;
    case 'x':
    case 'X':
      res |= LOG_FILES;
      break;
    case 's':
    case 'S':
      res |= LOG_SERV;
      break;
    case 'd':
    case 'D':
      res |= LOG_DEBUG;
      break;
    case 'v':
    case 'V':
      res |= debug_output ? LOG_SRVOUT : 0;
      break;
    case 't':
    case 'T':
      res |= debug_output ? LOG_BOTNET : 0;
      break;
    case 'h':
    case 'H':
      res |= debug_output ? LOG_BOTSHARE : 0;
      break;
    case '1':
      res |= LOG_LEV1;
      break;
    case '2':
      res |= LOG_LEV2;
      break;
    case '3':
      res |= LOG_LEV3;
      break;
    case '4':
      res |= LOG_LEV4;
      break;
    case '5':
      res |= LOG_LEV5;
      break;
    case '6':
      res |= LOG_LEV6;
      break;
    case '7':
      res |= LOG_LEV7;
      break;
    case '8':
      res |= LOG_LEV8;
      break;
    case '*':
      res |= LOG_ALL;
      break;
    }
  return res;
}

char *masktype(int x)
{
  static char s[24];		/* change this if you change the levels */
  char *p = s;

  if (x & LOG_MSGS)
    *p++ = 'm';
  if (x & LOG_PUBLIC)
    *p++ = 'p';
  if (x & LOG_JOIN)
    *p++ = 'j';
  if (x & LOG_MODES)
    *p++ = 'k';
  if (x & LOG_CMDS)
    *p++ = 'c';
  if (x & LOG_MISC)
    *p++ = 'o';
  if (x & LOG_BOTS)
    *p++ = 'b';
  if ((x & LOG_RAW) && use_console_r)
    *p++ = 'r';
  if (x & LOG_FILES)
    *p++ = 'x';
  if (x & LOG_SERV)
    *p++ = 's';
  if (x & LOG_DEBUG)
    *p++ = 'd';
  if (x & LOG_WALL)
    *p++ = 'w';
  if ((x & LOG_SRVOUT) && debug_output)
    *p++ = 'v';
  if ((x & LOG_BOTNET) && debug_output)
    *p++ = 't';
  if ((x & LOG_BOTSHARE) && debug_output)
    *p++ = 'h';
  if (x & LOG_LEV1)
    *p++ = '1';
  if (x & LOG_LEV2)
    *p++ = '2';
  if (x & LOG_LEV3)
    *p++ = '3';
  if (x & LOG_LEV4)
    *p++ = '4';
  if (x & LOG_LEV5)
    *p++ = '5';
  if (x & LOG_LEV6)
    *p++ = '6';
  if (x & LOG_LEV7)
    *p++ = '7';
  if (x & LOG_LEV8)
    *p++ = '8';
  if (p == s)
    *p++ = '-';
  *p = 0;
  return s;
}

/* some flags are mutually exclusive -- this roots them out */
int sanity_check(int atr)
{
  if ((atr & USER_BOT) && (atr & (USER_PARTY | USER_MASTER | USER_COMMON | USER_OWNER)))
    atr &= ~(USER_PARTY | USER_MASTER | USER_COMMON | USER_OWNER);
  
  /* If +d or +k, kill everything else */
  if (atr & (USER_DEOP|USER_KICK))
    atr &= ~(USER_OP|USER_MASTER|USER_OWNER|USER_PARTY|USER_FRIEND);
  if ((atr & USER_VOICE) && (atr & USER_QUIET))
    atr &= ~(USER_VOICE);
  if ((atr & USER_GVOICE) && (atr & USER_QUIET))
    atr &= ~(USER_GVOICE);
  /* can't be owner without also being master */
  if (atr & USER_OWNER)
    atr |= USER_MASTER;
  /* master implies botmaster, op, friend and janitor */
  if (atr & USER_MASTER)
    atr |= USER_OP | USER_FRIEND;
  /* can't be botnet master without party-line access */
  if (atr & USER_MASTER)
    atr |= USER_PARTY;
  return atr;
}

/* sanity check on channel attributes */
int chan_sanity_check(int chatr, int atr)
{
  if ((chatr & USER_OP) && (chatr & USER_DEOP))
    chatr &= ~(USER_OP);
  if ((chatr & USER_VOICE) && (chatr & USER_QUIET))
    chatr &= ~(USER_VOICE);
  if ((chatr & USER_GVOICE) && (chatr & USER_QUIET))
    chatr &= ~(USER_GVOICE);
  /* can't be channel owner without also being channel master */
  if (chatr & USER_OWNER)
    chatr |= USER_MASTER;
  /* master implies friend & op */
  if (chatr & USER_MASTER)
    chatr |= USER_OP | USER_FRIEND;
  return chatr;
}

/* get icon symbol for a user (depending on access level)
 * (*)owner on any channel
 * (+)master on any channel
 * (%) botnet master
 * (@) op on any channel
 * (-) other */
char geticon(int idx)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

  if (!dcc[idx].user)
    return '-';
  get_user_flagrec(dcc[idx].user, &fr, 0);
  if (chan_owner(fr))
    return '*';
  if (chan_master(fr))
    return '+';
  if (chan_op(fr))
    return '@';
  return '-';
}

void break_down_flags(char *string, struct flag_record *plus, struct flag_record *minus)
{
  struct flag_record *which = plus;
  int mode = 0;			/* 0 = glob, 1 = chan, 2 = bot */
  int flags = plus->match;

  if (!(flags & FR_GLOBAL)) {
    if (flags & FR_CHAN)
      mode = 1;
    else
      return;			/* we dont actually want any..huh? */
  }
  bzero(plus, sizeof(struct flag_record));

  if (minus)
    bzero(minus, sizeof(struct flag_record));

  plus->match = FR_OR;		/* befault binding type OR */
  while (*string) {
    switch (*string) {
    case '+':
      which = plus;
      break;
    case '-':
      which = minus ? minus : plus;
      break;
    case '|':
    case '&':
      if (mode == 0) {
	if (*string == '|')
	  plus->match = FR_OR;
	else
	  plus->match = FR_AND;
      }
      which = plus;
      mode++;
      if ((mode == 2) && !(flags & (FR_CHAN)))
	string = "";
      else if (mode == 3)
	mode = 1;
      break;
    default:
      if ((*string >= 'a') && (*string <= 'z')) {
	switch (mode) {
	case 0:
	  which->global |=1 << (*string - 'a');

	  break;
	case 1:
	  which->chan |= 1 << (*string - 'a');
	  break;
	}
      } else if ((*string >= 'A') && (*string <= 'Z')) {
	switch (mode) {
	case 0:
	  which->udef_global |= 1 << (*string - 'A');
	  break;
	case 1:
	  which->udef_chan |= 1 << (*string - 'A');
	  break;
	}
      } else if ((*string >= '0') && (*string <= '9')) {
	switch (mode) {
	  /* map 0->9 to A->K for glob/chan so they are not lost */
	case 0:
	  which->udef_global |= 1 << (*string - '0');
	  break;
	case 1:
	  which->udef_chan |= 1 << (*string - '0');
	  break;
	}
      }
    }
    string++;
  }
  for (which = plus; which; which = (which == plus ? minus : 0)) {
    which->global &=USER_VALID;

    which->udef_global &= 0x03ffffff;
    which->chan &= CHAN_VALID;
    which->udef_chan &= 0x03ffffff;
  }
  plus->match |= flags;
  if (minus) {
    minus->match |= flags;
    if (!(plus->match & (FR_AND | FR_OR)))
      plus->match |= FR_OR;
  }
}

int flag2str(char *string, int bot, int udef)
{
  char x = 'a',
   *old = string;

  while (bot && (x <= 'z')) {
    if (bot & 1)
      *string++ = x;
    x++;
    bot = bot >> 1;
  }
  x = 'A';
  while (udef && (x <= 'Z')) {
    if (udef & 1)
      *string++ = x;
    udef = udef >> 1;
    x++;
  }
  if (string == old)
    *string++ = '-';
  return string - old;
}

int build_flags(char *string, struct flag_record *plus, struct flag_record *minus)
{
  char *old = string;

  if (plus->match & FR_GLOBAL) {
    if (minus && (plus->global ||plus->udef_global))
      *string++ = '+';
    string += flag2str(string, plus->global, plus->udef_global);

    if (minus && (minus->global ||minus->udef_global)) {
      *string++ = '-';
      string += flag2str(string, minus->global, minus->udef_global);
    }
  }
  if (plus->match & FR_CHAN) {
    if (plus->match & (FR_GLOBAL))
      *string++ = (plus->match & FR_AND) ? '&' : '|';
    if (minus && (plus->chan || plus->udef_chan))
      *string++ = '+';
    string += flag2str(string, plus->chan, plus->udef_chan);
    if (minus && (minus->chan || minus->udef_chan)) {
      *string++ = '-';
      string += flag2str(string, minus->global, minus->udef_chan);
    }
  }
  if (string == old) {
    *string++ = '-';
    *string = 0;
    return 0;
  }
  *string = 0;
  return string - old;
}

int flagrec_ok(struct flag_record *req, struct flag_record *have)
{
  if (req->match & FR_AND) {
    return flagrec_eq(req, have);
  } else if (req->match & FR_OR) {
    int hav = have->global;

    /* exception 1 - global +d/+k cant use -|-, unless they are +p */
    if (!req->chan && !req->global &&!req->udef_global && !req->udef_chan) {
      if (!allow_dk_cmds) {
	if (glob_party(*have))
	  return 1;
	if (glob_kick(*have) || chan_kick(*have))
	  return 0;		/* +k cant use -|- commands */
	if (glob_deop(*have) || chan_deop(*have))
	  return 0;		/* neither can +d's */
      }
      return 1;
    }
    /* the +n/+m checks arent needed anymore since +n/+m
     * automatically add lower flags */
    if (hav & req->global)
      return 1;
    if (have->chan & req->chan)
      return 1;
    if (have->udef_global & req->udef_global)
      return 1;
    if (have->udef_chan & req->udef_chan)
      return 1;
    return 0;
  }
  return 0;			/* fr0k3 binding, dont pass it */
}

int flagrec_eq(struct flag_record *req, struct flag_record *have)
{
  if (req->match & FR_AND) {
    if (req->match & FR_GLOBAL) {
      if ((req->global &have->global) !=req->global)
	return 0;
      if ((req->udef_global & have->udef_global) != req->udef_global)
	return 0;
    }
    if (req->match & FR_CHAN) {
      if ((req->chan & have->chan) != req->chan)
	return 0;
      if ((req->udef_chan & have->udef_chan) != req->udef_chan)
	return 0;
    }
    return 1;
  } else if (req->match & FR_OR) {
    if (!req->chan && !req->global &&!req->udef_chan && !req->udef_global)
      return 1;
    if (req->match & FR_GLOBAL) {
      if (have->global &req->global)
	return 1;
      if (have->udef_global & req->udef_global)
	return 1;
    }
    if (req->match & FR_CHAN) {
      if (have->chan & req->chan)
	return 1;
      if (have->udef_chan & req->udef_chan)
	return 1;
    }
    return 0;
  }
  return 0;			/* fr0k3 binding, dont pass it */
}

void set_user_flagrec(struct userrec *u, struct flag_record *fr, char *chname)
{
  struct chanuserrec *cr = NULL;
  int oldflags = fr->match;
  char buffer[100];
  struct chanset_t *ch;

  if (!u)
    return;
  if (oldflags & FR_GLOBAL) {
    if ( (u->flags & USER_BOT) && (u->flags & USER_OP) && !(fr->global & USER_OP)) {
      int i=nextbot(u->handle);
      if ((i>=0) && (!strcasecmp2(dcc[i].nick, u->handle))) {
	botunlink(-1, u->handle, STR("Rejected"));
      }
    }
    u->flags = fr->global;

    u->flags_udef = fr->udef_global;
    if (!noshare && !(u->flags & USER_UNSHARED)) {
      fr->match = FR_GLOBAL;
      build_flags(buffer, fr, NULL);
      shareout(NULL, STR("a %s %s\n"), u->handle, buffer);
    }
  }
  /* dont share bot attrs */
  if ((oldflags & FR_CHAN) && chname) {
    for (cr = u->chanrec; cr; cr = cr->next)
      if (!rfc_casecmp(chname, cr->channel))
	break;
    ch = findchan(chname);
    if (!cr && ch) {
      cr = user_malloc(sizeof(struct chanuserrec));
      bzero(cr, sizeof(struct chanuserrec));

      cr->next = u->chanrec;
      u->chanrec = cr;
      strncpy0(cr->channel, chname, 80);
      cr->channel[80] = 0;
    }
    if (cr && ch) {
      cr->flags = fr->chan;
      cr->flags_udef = fr->udef_chan;
      if (!noshare && !(u->flags & USER_UNSHARED)) {
	fr->match = FR_CHAN;
	build_flags(buffer, fr, NULL);
	shareout(ch, STR("a %s %s %s\n"), u->handle, buffer, chname);
      }
    }
  }
  fr->match = oldflags;
}

void get_user_flagrec(struct userrec *u, struct flag_record *fr, char *chname)
{
  struct chanuserrec *cr = NULL;

  if (!u) {
    fr->global = fr->udef_global = fr->chan = fr->udef_chan = 0;

    return;
  }
  if (fr->match & FR_GLOBAL) {
    fr->global = u->flags;

    fr->udef_global = u->flags_udef;
  } else {
    fr->global = 0;

    fr->udef_global = 0;
  }
  if (fr->match & FR_CHAN) {
    if (fr->match & FR_ANYWH) {
      fr->chan = u->flags;
      fr->udef_chan = u->flags_udef;
      for (cr = u->chanrec; cr; cr = cr->next)
	if (findchan(cr->channel)) {
	  fr->chan |= cr->flags;
	  fr->udef_chan |= cr->flags_udef;
	}
    } else {
      if (chname)
	for (cr = u->chanrec; cr; cr = cr->next)
	  if (!rfc_casecmp(chname, cr->channel))
	    break;
      if (chname && cr) {
	fr->chan = cr->flags;
	fr->udef_chan = cr->flags_udef;
      } else {
	fr->chan = 0;
	fr->udef_chan = 0;
      }
    }
  }
}
