/*
 * console.c -- part of console.mod
 *   saved console settings based on console.tcl
 *   by cmwagner/billyjoe/D. Senso
 *
 */

#include "console.h"
#include "src/common.h"
#include "src/mod/share.mod/share.h"
#include "src/tclhash.h"
#include "src/tandem.h"
#include "src/cmds.h"
#include "src/users.h"
#include "src/userent.h"
#include "src/botmsg.h"
#include "src/userrec.h"
#include "src/users.h"
#include "src/misc.h"
#include "src/core_binds.h"

static int console_autosave = 1;
static int info_party = 1;

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

static int
console_unpack(struct userrec *u, struct user_entry *e)
{
  struct console_info *ci = NULL;
  char *par = NULL, *arg = NULL;

  ci = (struct console_info *) calloc(1, sizeof(struct console_info));

  par = e->u.list->extra;
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

static int
console_kill(struct user_entry *e)
{
  struct console_info *i = (struct console_info *) e->u.extra;

  free(i->channel);
  free(i);
  free(e);
  return 1;
}

#ifdef HUB
static int
console_write_userfile(FILE * f, struct userrec *u, struct user_entry *e)
{
  struct console_info *i = e->u.extra;

  if (lfprintf(f, "--CONSOLE %s %s %s %d %d %d %d %d %d %d %d\n",
               i->channel, masktype(i->conflags),
               stripmasktype(i->stripflags), i->echoflags,
               i->page, i->conchan, i->color, i->banner, i->channels, i->bots, i->whom) == EOF)
    return 0;
  return 1;
}
#endif /* HUB */

static int
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
    ci = e->u.extra = (struct console_info *) buf;
  }

  if (!noshare && !u->bot) {
    char string[501] = "";

    egg_snprintf(string, sizeof string, "%s %s %s %d %d %d %d %d %d %d %d", ci->channel,
                 masktype(ci->conflags), stripmasktype(ci->stripflags), ci->echoflags, ci->page, ci->conchan,
                 ci->color, ci->banner, ci->channels, ci->bots, ci->whom);
    /* shareout("c %s %s %s\n", e->type->name, u->handle, string); */
    shareout("c CONSOLE %s %s\n", u->handle, string);
  }
  return 1;
}

static int
console_gotshare(struct userrec *u, struct user_entry *e, char *par, int idx)
{
  struct console_info *ci = (struct console_info *) e->u.extra;
  char *arg = NULL;
  int i;

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
  /* now let's propogate to the dcc list */
  for (i = 0; i < dcc_total; i++) {
    if ((dcc[i].type == &DCC_CHAT) && !strcmp(dcc[i].user->handle, u->handle)) {
      if (ci->channel && ci->channel[0])
        strcpy(dcc[i].u.chat->con_chan, ci->channel);
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
    dprintf(idx, "  %s\n", CONSOLE_SAVED_SETTINGS);
    dprintf(idx, "    %s %s\n", CONSOLE_CHANNEL, i->channel);
    dprintf(idx, "    %s %s, %s %s, %s %s\n", CONSOLE_FLAGS,
            masktype(i->conflags), CONSOLE_STRIPFLAGS,
            stripmasktype(i->stripflags), CONSOLE_ECHO, i->echoflags ? CONSOLE_YES : CONSOLE_NO);
    dprintf(idx, "    %s %d, %s %s%d\n", CONSOLE_PAGE_SETTING, i->page,
            CONSOLE_CHANNEL2, (i->conchan < GLOBAL_CHANS) ? "" : "*", i->conchan % GLOBAL_CHANS);
    dprintf(idx, "    Color: %s\n", i->color ? "on" : "off");
    dprintf(idx, "    Login settings:\n");
    dprintf(idx, "     Banner: %s\n", i->banner ? "on" : "off");
    dprintf(idx, "     Channels: %s\n", i->channels ? "on" : "off");
    dprintf(idx, "     Bots: %s\n", i->bots ? "on" : "off");
    dprintf(idx, "     Whom: %s\n", i->whom ? "on" : "off");
  }
}

static struct user_entry_type USERENTRY_CONSOLE = {
  0,                            /* always 0 ;) */
  console_gotshare,
  console_unpack,
#ifdef HUB
  console_write_userfile,
#endif /* HUB */
  console_kill,
  NULL,
  console_set,
  console_display,
  "CONSOLE"
};

static int
console_chon(char *handle, int idx)
{
  struct console_info *i = (struct console_info *) get_user(&USERENTRY_CONSOLE, dcc[idx].user);

  if (dcc[idx].type == &DCC_CHAT) {
    if (i) {
      if (i->channel && i->channel[0])
        strcpy(dcc[idx].u.chat->con_chan, i->channel);
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
/* FIXME: Remove this after 1.1.8 */
      dcc[idx].status |= STAT_BANNER | STAT_CHANNELS | STAT_BOTS | STAT_WHOM;

    }
    if ((dcc[idx].u.chat->channel >= 0) && (dcc[idx].u.chat->channel < GLOBAL_CHANS)) {
      botnet_send_join_idx(idx);
    }
    if (info_party) {
      char *p = (char *) get_user(&USERENTRY_INFO, dcc[idx].user);

      if (p) {
        if (dcc[idx].u.chat->channel >= 0) {
          char x[1024] = "";

          chanout_but(-1, dcc[idx].u.chat->channel, "*** [%s] %s\n", dcc[idx].nick, p);
          simple_sprintf(x, "[%s] %s", dcc[idx].nick, p);
          botnet_send_chan(-1, conf.bot->nick, NULL, dcc[idx].u.chat->channel, x);
        }
      }
    }
  }
  return 0;
}

static int
console_store(int idx, char *par)
{
  struct console_info *i = (struct console_info *) get_user(&USERENTRY_CONSOLE, dcc[idx].user);

  if (!i) {
    i = (struct console_info *) calloc(1, sizeof(struct console_info));
  }
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
    dprintf(idx, "%s\n", CONSOLE_SAVED_SETTINGS2);
    dprintf(idx, "  %s %s\n", CONSOLE_CHANNEL, i->channel);
    dprintf(idx, "  %s %s, %s %s, %s %s\n", CONSOLE_FLAGS,
            masktype(i->conflags), CONSOLE_STRIPFLAGS,
            stripmasktype(i->stripflags), CONSOLE_ECHO, i->echoflags ? CONSOLE_YES : CONSOLE_NO);
    dprintf(idx, "  %s %d, %s %d\n", CONSOLE_PAGE_SETTING, i->page, CONSOLE_CHANNEL2, i->conchan);
    dprintf(idx, "    Color: %s\n", i->color ? "on" : "off");
    dprintf(idx, "    Login settings:\n");
    dprintf(idx, "     Banner: %s\n", i->banner ? "on" : "off");
    dprintf(idx, "     Channels: %s\n", i->channels ? "on" : "off");
    dprintf(idx, "     Bots: %s\n", i->bots ? "on" : "off");
    dprintf(idx, "     Whom: %s\n", i->whom ? "on" : "off");
  }
  set_user(&USERENTRY_CONSOLE, dcc[idx].user, i);
  dprintf(idx, "Console setting stored.\n");
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
  return 0;
}

/* cmds.c:cmd_console calls this, better than chof bind - drummer,07/25/1999 */
int
console_dostore(int idx)
{
  if (console_autosave)
    console_store(idx, NULL);
  return 0;
}

static cmd_t mychon[] = {
  {"*", "", console_chon, "console:chon"},
  {NULL, NULL, NULL, NULL}
};

static cmd_t mydcc[] = {
  {"store", "", console_store, NULL},
  {NULL, NULL, NULL, NULL}
};

void
console_init()
{
  add_builtins("dcc", mydcc);
  add_builtins("chon", mychon);

  USERENTRY_CONSOLE.get = def_get;
  add_entry_type(&USERENTRY_CONSOLE);
}
