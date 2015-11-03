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
 * console.c -- part of console.mod
 *   saved console settings based on console.tcl
 *   by cmwagner/billyjoe/D. Senso
 *
 */


#include "console.h"
#include "src/common.h"
#include "src/mod/share.mod/share.h"
#include "src/binds.h"
#include "src/tandem.h"
#include "src/cmds.h"
#include "src/users.h"
#include "src/userent.h"
#include "src/botmsg.h"
#include "src/userrec.h"
#include "src/users.h"
#include "src/misc.h"
#include "src/core_binds.h"
#include <bdlib/src/Stream.h>
#include <bdlib/src/String.h>

struct console_info {
  char *channel;
  int conflags;
  int stripflags;
  int echoflags;
  int page;
  int conchan;
  int color;
  int banner;
  int channels;
  int bots;
  int whom;
};

static bool
console_unpack(struct userrec *u, struct user_entry *e)
{
  struct console_info *ci = (struct console_info *) calloc(1, sizeof(struct console_info));
  char *par = e->u.list->extra, *arg = NULL;

  arg = newsplit(&par);
  ci->channel = strdup(arg);
  arg = newsplit(&par);
  ci->conflags = logmodes(arg);
  arg = newsplit(&par);
  ci->stripflags = stripmodes(arg);
  arg = newsplit(&par);
  ci->echoflags = (arg[0] == '1') ? 1 : 0;
  arg = newsplit(&par);
  ci->page = atoi(arg);
  arg = newsplit(&par);
  ci->conchan = atoi(arg);
  arg = newsplit(&par);
  ci->color = atoi(arg);
  arg = newsplit(&par);
  ci->banner = atoi(arg);
  arg = newsplit(&par);
  ci->channels = atoi(arg);
  arg = newsplit(&par);
  ci->bots = atoi(arg);
  arg = newsplit(&par);
  ci->whom = atoi(arg);

  list_type_kill(e->u.list);
  e->u.extra = ci;
  return 1;
}

static bool
console_kill(struct user_entry *e)
{
  struct console_info *i = (struct console_info *) e->u.extra;

  free(i->channel);
  free(i);
  free(e);
  return 1;
}

static void
console_write_userfile(bd::Stream& stream, const struct userrec *u, const struct user_entry *e, int idx)
{
  if (u->bot)
    return;

  struct console_info *i = (struct console_info *) e->u.extra;

  stream << bd::String::printf("--CONSOLE %s %s %s %d %d %d %d %d %d %d %d\n",
               i->channel, masktype(i->conflags),
               stripmasktype(i->stripflags), i->echoflags,
               i->page, i->conchan, i->color, i->banner, i->channels, i->bots, i->whom);
}

static bool
console_set(struct userrec *u, struct user_entry *e, void *buf)
{
  struct console_info *ci = (struct console_info *) e->u.extra;

  if (!ci && !buf)
    return 1;

  if (ci != buf) {
    if (ci) {
      free(ci->channel);
      free(ci);
    }
    ci = (struct console_info *) buf;
    e->u.extra = (struct console_info *) buf;
  }

  if (!noshare && !u->bot) {
    char string[501] = "";

    simple_snprintf(string, sizeof string, "%s %s %s %d %d %d %d %d %d %d %d", ci->channel,
                 masktype(ci->conflags), stripmasktype(ci->stripflags), ci->echoflags, ci->page, ci->conchan,
                 ci->color, ci->banner, ci->channels, ci->bots, ci->whom);
    /* shareout("c %s %s %s\n", e->type->name, u->handle, string); */
    shareout("c CONSOLE %s %s\n", u->handle, string);
  }
  return 1;
}

static bool
console_gotshare(struct userrec *u, struct user_entry *e, char *par, int idx)
{
  struct console_info *ci = (struct console_info *) e->u.extra;
  char *arg = NULL;

  arg = newsplit(&par);
  if (ci) {
    free(ci->channel);
    free(ci);
  }
  ci = (struct console_info *) calloc(1, sizeof(struct console_info));
  ci->channel = strdup(arg);
  arg = newsplit(&par);
  ci->conflags = logmodes(arg);
  arg = newsplit(&par);
  ci->stripflags = stripmodes(arg);
  arg = newsplit(&par);
  ci->echoflags = (arg[0] == '1') ? 1 : 0;
  arg = newsplit(&par);
  ci->page = atoi(arg);
  arg = newsplit(&par);
  ci->conchan = atoi(arg);
  arg = newsplit(&par);
  ci->color = atoi(arg);
  arg = newsplit(&par);
  ci->banner = atoi(arg);
  arg = newsplit(&par);
  ci->channels = atoi(arg);
  arg = newsplit(&par);
  ci->bots = atoi(arg);
  arg = newsplit(&par);
  ci->whom = atoi(arg);

  e->u.extra = ci;
  struct chat_info dummy;
  /* now let's propogate to the dcc list */
  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type == &DCC_CHAT) && !strcmp(dcc[i].user->handle, u->handle)) {
      if (ci->channel && ci->channel[0])
        strlcpy(dcc[i].u.chat->con_chan, ci->channel, sizeof(dummy.con_chan));
      dcc[i].u.chat->con_flags = ci->conflags;
      dcc[i].u.chat->strip_flags = ci->stripflags;
      if (ci->echoflags)
        dcc[i].status |= STAT_ECHO;
      else
        dcc[i].status &= ~STAT_ECHO;
      if (ci->page) {
        dcc[i].status |= STAT_PAGE;
        dcc[i].u.chat->max_line = ci->page;
        if (!dcc[i].u.chat->line_count)
          dcc[i].u.chat->current_lines = 0;
      }
      if (ci->color)
        dcc[i].status |= STAT_COLOR;
      else
        dcc[i].status &= ~STAT_COLOR;
      if (ci->banner)
        dcc[i].status |= STAT_BANNER;
      else
        dcc[i].status &= ~STAT_BANNER;
      if (ci->channels)
        dcc[i].status |= STAT_CHANNELS;
      else
        dcc[i].status &= ~STAT_CHANNELS;
      if (ci->bots)
        dcc[i].status |= STAT_BOTS;
      else
        dcc[i].status &= ~STAT_BOTS;
      if (ci->whom)
        dcc[i].status |= STAT_WHOM;
      else
        dcc[i].status &= ~STAT_WHOM;
    }
  }
  return 1;
}

static void
console_display(int idx, struct user_entry *e, struct userrec *u)
{
  struct console_info *i = (struct console_info *) e->u.extra;

  if (dcc[idx].user && (dcc[idx].user->flags & USER_MASTER)) {
    dprintf(idx, "  %s\n", "Saved Console Settings:");
    dprintf(idx, "    %s %s\n", "Channel:", i->channel);
    dprintf(idx, "    %s %s, %s %s, %s %s\n", "Console flags:",
            masktype(i->conflags), "",
            stripmasktype(i->stripflags), "Echo:", i->echoflags ? "yes" : "no");
    dprintf(idx, "    %s %d, %s %s%d\n", "Page setting:", i->page,
            "Console channel:", (i->conchan < GLOBAL_CHANS) ? "" : "*", i->conchan % GLOBAL_CHANS);
    dprintf(idx, "    Color: $b%s$b\n", i->color ? "on" : "off");
    dprintf(idx, "    Login settings:\n");
    dprintf(idx, "     Banner:   $b%-3s$b   Bots: $b%-3s$b\n", i->banner ? "on" : "off", i->bots ? "on" : "off");
    dprintf(idx, "     Channels: $b%-3s$b   Whom: $b%-3s$b\n", i->channels ? "on" : "off", i->whom ? "on" : "off");
  }
}

struct user_entry_type USERENTRY_CONSOLE = {
  0,                            /* always 0 ;) */
  console_gotshare,
  console_unpack,
  console_write_userfile,
  console_kill,
  def_get,
  console_set,
  console_display,
  "CONSOLE"
};

static int
console_chon(char *handle, int idx)
{
  if (dcc[idx].type == &DCC_CHAT) {
    struct console_info *i = (struct console_info *) get_user(&USERENTRY_CONSOLE, dcc[idx].user);

    if (i) {
      if (i->channel && i->channel[0]) {
        struct chat_info dummy;
        strlcpy(dcc[idx].u.chat->con_chan, i->channel, sizeof(dummy.con_chan));
      }
      dcc[idx].u.chat->con_flags = i->conflags;
      dcc[idx].u.chat->strip_flags = i->stripflags;
      if (i->echoflags)
        dcc[idx].status |= STAT_ECHO;
      else
        dcc[idx].status &= ~STAT_ECHO;
      if (i->page) {
        dcc[idx].status |= STAT_PAGE;
        dcc[idx].u.chat->max_line = i->page;
        if (!dcc[idx].u.chat->line_count)
          dcc[idx].u.chat->current_lines = 0;
      }
      if (i->color)
        dcc[idx].status |= STAT_COLOR;
      else
        dcc[idx].status &= ~STAT_COLOR;
      if (i->banner)
        dcc[idx].status |= STAT_BANNER;
      else
        dcc[idx].status &= ~STAT_BANNER;
      if (i->channels)
        dcc[idx].status |= STAT_CHANNELS;
      else
        dcc[idx].status &= ~STAT_CHANNELS;
      if (i->bots)
        dcc[idx].status |= STAT_BOTS;
      else
        dcc[idx].status &= ~STAT_BOTS;
      if (i->whom)
        dcc[idx].status |= STAT_WHOM;
      else
        dcc[idx].status &= ~STAT_WHOM;
    }
    if ((dcc[idx].u.chat->channel >= 0) && (dcc[idx].u.chat->channel < GLOBAL_CHANS)) {
      botnet_send_join_idx(idx);
    }
  }
  return 0;
}

static int
console_store(int idx, char *par, bool displaySave)
{
  struct console_info *i = (struct console_info *) get_user(&USERENTRY_CONSOLE, dcc[idx].user);

  if (!i) 
    i = (struct console_info *) calloc(1, sizeof(struct console_info));
  
  if (i->channel)
    free(i->channel);
  i->channel = strdup(dcc[idx].u.chat->con_chan);
  i->conflags = dcc[idx].u.chat->con_flags;
  i->stripflags = dcc[idx].u.chat->strip_flags;
  i->echoflags = (dcc[idx].status & STAT_ECHO) ? 1 : 0;
  if (dcc[idx].status & STAT_PAGE)
    i->page = dcc[idx].u.chat->max_line;
  else
    i->page = 0;
  if (dcc[idx].status & STAT_COLOR)
    i->color = 1;
  else
    i->color = 0;
  if (dcc[idx].status & STAT_BANNER)
    i->banner = 1;
  else
    i->banner = 0;
  if (dcc[idx].status & STAT_CHANNELS)
    i->channels = 1;
  else
    i->channels = 0;
  if (dcc[idx].status & STAT_BOTS)
    i->bots = 1;
  else
    i->bots = 0;
  if (dcc[idx].status & STAT_WHOM)
    i->whom = 1;
  else
    i->whom = 0;

  i->conchan = dcc[idx].u.chat->channel;
  if (par) {
    dprintf(idx, "%s\n", "Saved your Console Settings:");
    dprintf(idx, "  %s %s\n", "Channel:", i->channel);
    dprintf(idx, "  %s %s, %s %s, %s %s\n", "Console flags:",
            masktype(i->conflags), "",
            stripmasktype(i->stripflags), "Echo:", i->echoflags ? "yes" : "no");
    dprintf(idx, "  %s %d, %s %d\n", "Page setting:", i->page, "Console channel:", i->conchan);
    dprintf(idx, "    Color: $b%s$b\n", i->color ? "on" : "off");
    dprintf(idx, "    Login settings:\n");
    dprintf(idx, "    Login settings:\n");
    dprintf(idx, "     Banner:   $b%-3s$b   Bots: $b%-3s$b\n", i->banner ? "on" : "off", i->bots ? "on" : "off");
    dprintf(idx, "     Channels: $b%-3s$b   Whom: $b%-3s$b\n", i->channels ? "on" : "off", i->whom ? "on" : "off");

  }
  set_user(&USERENTRY_CONSOLE, dcc[idx].user, i);
  dprintf(idx, "Console setting stored.\n");
  if (conf.bot->hub)
    write_userfile(displaySave ? idx : -1);
  return 0;
}

/* cmds.c:cmd_console calls this, better than chof bind - drummer,07/25/1999 */
void
console_dostore(int idx, bool displaySave)
{
  console_store(idx, NULL, displaySave);
  return;
}

static cmd_t mychon[] = {
  {"*", "", (Function) console_chon, "console:chon", 0},
  {NULL, NULL, NULL, NULL, 0}
};

static cmd_t mydcc[] = {
  {"store", "", (Function) console_store, NULL, 0},
  {NULL, NULL, NULL, NULL, 0}
};

void
console_init()
{
  add_builtins("dcc", mydcc);
  add_builtins("chon", mychon);

  add_entry_type(&USERENTRY_CONSOLE);
}
/* vim: set sts=2 sw=2 ts=8 et: */
