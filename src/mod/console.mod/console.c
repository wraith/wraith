/*
 * console.c -- part of console.mod
 *   saved console settings based on console.tcl
 *   by cmwagner/billyjoe/D. Senso
 *
 */

#define MODULE_NAME "console"
#define MAKING_CONSOLE
#include "src/mod/module.h"
#include <stdlib.h>
#include "console.h"

static Function *global = NULL;
static int console_autosave = 1;
static int info_party = 1;

struct console_info {
  char *channel;
  int conflags;
  int stripflags;
  int echoflags;
  int page;
  int conchan;
  int colour;
};

static struct user_entry_type USERENTRY_CONSOLE;


static int console_unpack(struct userrec *u, struct user_entry *e)
{
  struct console_info *ci = malloc(sizeof(struct console_info));
  char *par, *arg;

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
  ci->colour = atoi(arg);
  list_type_kill(e->u.list);
  e->u.extra = ci;
  return 1;
}

static int console_pack(struct userrec *u, struct user_entry *e)
{
  char work[1024];
  struct console_info *ci;
  int l;

  ci = (struct console_info *) e->u.extra;

  l = simple_sprintf(work, "%s %s %s %d %d %d %d",
		     ci->channel, masktype(ci->conflags),
		     stripmasktype(ci->stripflags), ci->echoflags,
		     ci->page, ci->conchan, ci->colour);

  e->u.list = malloc(sizeof(struct list_type));
  e->u.list->next = NULL;
  e->u.list->extra = malloc(l + 1);
  strcpy(e->u.list->extra, work);

  free(ci->channel);
  free(ci);
  return 1;
}

static int console_kill(struct user_entry *e)
{
  struct console_info *i = e->u.extra;

  free(i->channel);
  free(i);
  free(e);
  return 1;
}

static int console_write_userfile(FILE *f, struct userrec *u,
				  struct user_entry *e)
{
  struct console_info *i = e->u.extra;

  if (lfprintf(f, "--CONSOLE %s %s %s %d %d %d %d\n",
	      i->channel, masktype(i->conflags),
	      stripmasktype(i->stripflags), i->echoflags,
	      i->page, i->conchan, i->colour) == EOF)
    return 0;
  return 1;
}

static int console_set(struct userrec *u, struct user_entry *e, void *buf)
{
  struct console_info *ci = (struct console_info *) e->u.extra;

  if (!ci && !buf)
    return 1;

  if (ci != buf) {
    if (ci) {
      free(ci->channel);
      free(ci);
    }
    ci = e->u.extra = buf;
  }

  if (!noshare && !(u->flags & (USER_BOT | USER_UNSHARED))) {
    char string[501];    
    egg_snprintf(string, sizeof string, "%s %s %s %d %d %d %d", ci->channel, masktype(ci->conflags), 
                                    stripmasktype(ci->stripflags), ci->echoflags, ci->page, ci->conchan,
                                    ci->colour);
    /* shareout(NULL, "c %s %s %s\n", e->type->name, u->handle, string); */
    shareout(NULL, "c CONSOLE %s %s\n", u->handle, string);
  }
  return 1;
}

static int console_gotshare(struct userrec *u, struct user_entry *e, char *par, int idx)
{
  struct console_info *ci = (struct console_info *) e->u.extra;
  char *arg;
  int i;

  arg = newsplit(&par);
  if (ci) {
    free(ci->channel);
    free(ci);
  }
  ci = malloc(sizeof(struct console_info));
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
  ci->colour = atoi(arg);
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
      if (ci->colour)
        dcc[i].status |= (STAT_COLOR);
      else
        dcc[i].status &= ~(STAT_COLOR);
    }
  }
  return 1;
}

static void console_display(int idx, struct user_entry *e, struct userrec *u)
{
  struct console_info *i = e->u.extra;
  char tmp[100];
  if (dcc[idx].user && (dcc[idx].user->flags & USER_MASTER)) {
    dprintf(idx, "  %s\n", CONSOLE_SAVED_SETTINGS);
    dprintf(idx, "    %s %s\n", CONSOLE_CHANNEL, i->channel);
    dprintf(idx, "    %s %s, %s %s, %s %s\n", CONSOLE_FLAGS,
    	masktype(i->conflags), CONSOLE_STRIPFLAGS,
	    stripmasktype(i->stripflags), CONSOLE_ECHO,
	    i->echoflags ? CONSOLE_YES : CONSOLE_NO);
    dprintf(idx, "    %s %d, %s %s%d\n", CONSOLE_PAGE_SETTING, i->page,
            CONSOLE_CHANNEL2, (i->conchan < GLOBAL_CHANS) ? "" : "*",
            i->conchan % GLOBAL_CHANS);
    sprintf(tmp, "    Color:");
    if (i->colour == 1)
     sprintf(tmp, "%s on", tmp);
    else
     sprintf(tmp, "%s off", tmp);
    dprintf(idx, "%s\n", tmp);
  }
}

static int console_dupuser(struct userrec *new, struct userrec *old,
			   struct user_entry *e)
{
  struct console_info *i = e->u.extra, *j;

  j = malloc(sizeof(struct console_info));
  my_memcpy(j, i, sizeof(struct console_info));

  j->channel = strdup(i->channel);
  return set_user(e->type, new, j);
}

static struct user_entry_type USERENTRY_CONSOLE =
{
  0,				/* always 0 ;) */
  console_gotshare,
  console_dupuser,
  console_unpack,
  console_pack,
  console_write_userfile,
  console_kill,
  NULL,
  console_set,
  console_display,
  "CONSOLE"
};

static int console_chon(char *handle, int idx)
{
  struct console_info *i = get_user(&USERENTRY_CONSOLE, dcc[idx].user);

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
      if (i->colour)
        dcc[idx].status |= (STAT_COLOR);
      else
        dcc[idx].status &= ~(STAT_COLOR);
    }
    if ((dcc[idx].u.chat->channel >= 0) &&
	(dcc[idx].u.chat->channel < GLOBAL_CHANS)) {
      botnet_send_join_idx(idx, -1);
      check_bind_chjn(botnetnick, dcc[idx].nick, dcc[idx].u.chat->channel,
		     geticon(idx), dcc[idx].sock, dcc[idx].host);
    }
    if (info_party) {
      char *p = get_user(&USERENTRY_INFO, dcc[idx].user);

      if (p) {
	if (dcc[idx].u.chat->channel >= 0) {
	    char x[1024];

	    chanout_but(-1, dcc[idx].u.chat->channel,
			"*** [%s] %s\n", dcc[idx].nick, p);
	    simple_sprintf(x, "[%s] %s", dcc[idx].nick, p);
	    botnet_send_chan(-1, botnetnick, NULL,
			     dcc[idx].u.chat->channel, x);
	}
      }
    }
  }
  return 0;
}

static int console_store(struct userrec *u, int idx, char *par)
{
  struct console_info *i = get_user(&USERENTRY_CONSOLE, u);

  if (!i) {
    i = malloc(sizeof(struct console_info));
    egg_bzero(i, sizeof(struct console_info));
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
    i->colour = 1;
  else
    i->colour = 0;
  i->conchan = dcc[idx].u.chat->channel;
  if (par) {
    char tmp[100];
    dprintf(idx, "%s\n", CONSOLE_SAVED_SETTINGS2);
    dprintf(idx, "  %s %s\n", CONSOLE_CHANNEL, i->channel);
    dprintf(idx, "  %s %s, %s %s, %s %s\n", CONSOLE_FLAGS,
	    masktype(i->conflags), CONSOLE_STRIPFLAGS,
	    stripmasktype(i->stripflags), CONSOLE_ECHO,
	    i->echoflags ? CONSOLE_YES : CONSOLE_NO);
    dprintf(idx, "  %s %d, %s %d\n", CONSOLE_PAGE_SETTING, i->page,
            CONSOLE_CHANNEL2, i->conchan);
    sprintf(tmp, "    Color:");
    if (i->colour == 1)
     sprintf(tmp, "%s on", tmp);
    else
     sprintf(tmp, "%s off", tmp);
    dprintf(idx, "%s\n", tmp);
  }
  set_user(&USERENTRY_CONSOLE, u, i);
  dprintf(idx, "Console setting stored.\n");
#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
  return 0;
}

/* cmds.c:cmd_console calls this, better than chof bind - drummer,07/25/1999 */
static int console_dostore(int idx)
{
  if (console_autosave)
    console_store(dcc[idx].user, idx, NULL);
  return 0;
}

static cmd_t mychon[] =
{
  {"*",		"",	console_chon,		"console:chon"},
  {NULL,	NULL,	NULL,			NULL}
};

static cmd_t mydcc[] =
{
  {"store",	"",	console_store,		NULL},
  {NULL,	NULL,	NULL,			NULL}
};

EXPORT_SCOPE char *console_start();

static Function console_table[] =
{
  (Function) console_start,
  (Function) NULL,
  (Function) NULL,
  (Function) NULL,
  (Function) console_dostore,
};

char *console_start(Function * global_funcs)
{
  global = global_funcs;

  module_register(MODULE_NAME, console_table, 1, 1);

  add_builtins("dcc", mydcc);
  add_builtins("chon", mychon);

  USERENTRY_CONSOLE.get = def_get;
  add_entry_type(&USERENTRY_CONSOLE);
  return NULL;
}
