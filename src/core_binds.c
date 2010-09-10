/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2010 Bryan Drewery
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
 * core_binds.c -- handles:
 *
 *   binds for the CORE
 *
 */


#include "common.h"
#include "dccutil.h"
#include "auth.h"
#include "core_binds.h"
#include "userrec.h"
#include "main.h"
#include "settings.h"
#include "set.h"
#include "users.h"
#include "misc.h"
#include "binds.h"
#include "dcc.h"
#include "cmds.h"

extern cmd_t 		C_dcc[];

static bind_table_t *BT_away = NULL, *BT_dcc = NULL;
static bind_table_t *BT_note = NULL;
static bind_table_t *BT_bot = NULL, *BT_nkch = NULL, *BT_chon = NULL;
static bind_table_t *BT_time = NULL;

void core_binds_init()
{
        BT_away = bind_table_add("away", 3, "sis", MATCH_MASK, BIND_STACKABLE);
        BT_bot = bind_table_add("bot", 3, "sss", MATCH_EXACT, 0);
        BT_chon = bind_table_add("chon", 2, "si", MATCH_MASK | MATCH_FLAGS, BIND_STACKABLE);
        BT_dcc = bind_table_add("dcc", 2, "is", MATCH_PARTIAL | MATCH_FLAGS, 0);
	bzero(&cmdlist, 500);
        add_builtins("dcc", C_dcc);
        BT_nkch = bind_table_add("nkch", 2, "ss", MATCH_MASK, BIND_STACKABLE);
        BT_note = bind_table_add("note", 3 , "sss", MATCH_EXACT, 0);
	BT_time = bind_table_add("time", 5, "iiiii", MATCH_MASK, BIND_STACKABLE);
}

bool check_aliases(int idx, const char *cmd, const char *args)
{
  char *a = NULL, *p = NULL, *aliasp = NULL, *aliasdup = NULL, *argsp = NULL, *argsdup = NULL;
  bool found = 0;
  bind_entry_t *entry = NULL;
  bind_table_t *table = NULL;

  aliasp = aliasdup = strdup(alias);
  if (args && args[0])
    argsp = argsdup = strdup(args);

  while ((a = strsep(&aliasdup, ","))) { //a = entire alias "alias cmd params"
    p = newsplit(&a);   //p = alias //a = cmd params
    if (!strcasecmp(p, cmd)) { //a match on the cmd we were given!
      p = newsplit(&a); //p = cmd //a = params

      /*
         p = cmd to be executed
         a = params to be passed to the cmd from the alias listing
         args = params given by the user -- may be a cmdpass that needs inserting before the alias params
       */

      /* Simple loop check */
      if (!strcasecmp(cmd, p)) {
        putlog(LOG_WARN, "*", "Loop detected in alias '%s'", p);
        if (argsp)
          free(argsp);
        return 0;
      }

      /* Sanity check - Aliases cannot reference other aliases */
      bool find = 0;
      table = bind_table_lookup("dcc");
      for (entry = table->entries; entry && entry->next; entry = entry->next) {
        if (!strncasecmp(p, entry->mask, strlen(p))) {
          find = 1;
          break;
        }
      }

      if (!find) {
        /* Does the cmd exist though? (Hub-only cmd from a leaf or a leaf-only cmd from a hub, or restricted cmd) */
        if (findcmd(cmd, 0)) {
          if (argsp)
            free(argsp);
          free(aliasp);
          return 0; /* Show bad cmd */
        } else {
          /* nope, show alias error */
          dprintf(idx, "'%s' is an invalid alias: references alias '%s'.\n", cmd, p);
          putlog(LOG_ERROR, "*", "Invalid alias '%s' attempted: references alias '%s'.", cmd, p);
          if (argsp)
            free(argsp);
          free(aliasp);
          return 1; /* Alias was found -- just not accepted */
        }
      }

      char *myargs = NULL, *pass = NULL;
      size_t size = 0;

      found = 1;

      size = strlen(a) + 1 + (argsdup ? strlen(argsdup) : 0) + 1 + 2;
      myargs = (char *) calloc(1, size);

      /* Rewrite the cmd including the inserted cmdpass if the cmd has one, and the user provided any param */
      if (argsdup && argsdup[0] && has_cmd_pass(p)) {
        pass = newsplit(&argsdup);
        strlcpy(myargs, pass, size);
        if (a && a[0]) {
          strlcat(myargs, " ", size);
          strlcat(myargs, a, size);
        }
        if (argsdup[0]) { /* was split */
          strlcat(myargs, " ", size);
          strlcat(myargs, argsdup, size);
        }
      } else {
        /* Otherwise, just construct it based on cmd and params if provided */
        if (argsdup && argsdup[0]) {
          strlcpy(myargs, a, size);
          strlcat(myargs, " ", size);
          strlcat(myargs, argsdup, size);
        } else 
          strlcpy(myargs, a, size);
      }

      if (a && a[0])
        putlog(LOG_CMDS, "*", "@ #%s# [%s -> %s %s] ...", dcc[idx].nick, cmd, p, a);
      else
        putlog(LOG_CMDS, "*", "@ #%s# [%s -> %s] ...", dcc[idx].nick, cmd, p);
      check_bind_dcc(p, idx, myargs);

      if (myargs)
        free(myargs);
      break;
    }
  }

  free(aliasp);
  if (argsp)
    free(argsp);

  return found;
}

int check_bind_dcc(const char *cmd, int idx, const char *text)
{
  return real_check_bind_dcc(cmd, idx, text, NULL);
}

int real_check_bind_dcc(const char *cmd, int idx, const char *text, Auth *auth)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN | FR_ANYCH, 0, 0, 0 };
  bind_entry_t *entry = NULL;
  bind_table_t *table = NULL;
  char *args = strdup(text);
  size_t args_siz = strlen(args) + 1;

  get_user_flagrec(dcc[idx].user, &fr, NULL);

  table = bind_table_lookup("dcc");
  size_t cmdlen = strlen(cmd);

  int hits = 0;

  for (entry = table->entries; entry && entry->next; entry = entry->next)
    if (!strncasecmp(cmd, entry->mask, cmdlen))
      ++hits;
 
  if (hits == 1) {
    for (entry = table->entries; entry && entry->next; entry = entry->next) {
      if (!strncasecmp(cmd, entry->mask, cmdlen)) {
        if (has_cmd_pass(entry->mask)) {
          if (flagrec_ok(&entry->user_flags, &fr)) {
            char *p = NULL, work[1024] = "", pass[MAXPASSLEN + 1] = "";

            p = strchr(args, ' ');
            if (p)
              *p = 0;
            strlcpy(pass, args, sizeof(pass));

            if (check_cmd_pass(entry->mask, pass)) {
              if (p)
                *p = ' ';
              strlcpy(work, args, sizeof(work));
              p = work;
              newsplit(&p);
              strlcpy(args, p, args_siz);
            } else {
              dprintf(idx, "Invalid command password.\n");
              dprintf(idx, "Use: $b%scommand <password> [arguments]$b\n", (dcc[idx].u.chat->channel >= 0) ? settings.dcc_prefix : "");
              if (p)
                p++;
              putlog(LOG_CMDS, "*", "$ #%s# %s **hidden** %s", dcc[idx].nick, entry->mask, p && *p ? p : "");
              putlog(LOG_MISC, "*", "%s attempted %s%s with missing or incorrect command password", dcc[idx].nick, settings.dcc_prefix, entry->mask);
              free(args);
              return 0;
            }
          } else {
            putlog(LOG_CMDS, "*", "! #%s# %s %s", dcc[idx].nick, cmd, args);
            dprintf(idx, "What?  You need '%shelp'\n", (dcc[idx].u.chat->channel >= 0) ? settings.dcc_prefix : "");
            free(args);
            return 0;
          }
        }
        break;
      }
    }
  }

  if (entry && auth) {
    if (!(entry->cflags & AUTH))
      return 0;
  }

  hits = 0;
  bool log_bad = 0;

  int ret = check_bind_hits(BT_dcc, cmd, &fr, &hits, idx, args);

  if (hits != 1)
    log_bad = 1;

  if (hits == 0) {
    if (!check_aliases(idx, cmd, args)) {
      log_bad = 1;
      dprintf(idx, "What?  You need '%shelp'\n", (dcc[idx].u.chat->channel >= 0) ? settings.dcc_prefix : "");
    } else
      log_bad = 0;
  } else if (hits > 1)
    dprintf(idx, "Ambiguous command.\n");

  if (log_bad)
    putlog(LOG_CMDS, "*", "! #%s# %s %s", dcc[idx].nick, cmd, args);

  free(args);
  return ret;
}

void check_bind_bot(const char *nick, const char *code, const char *param)
{
  char *mynick = NULL, *myparam = NULL, *p1 = NULL, *p2 = NULL;

  mynick = p1 = strdup(nick);
  myparam = p2 = strdup(param);

  check_bind(BT_bot, code, NULL, mynick, code, myparam);
  free(p1);
  free(p2);
}

void check_bind_chon(char *hand, int idx)
{
  struct flag_record     fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  struct userrec        *u = NULL;

  u = get_user_by_handle(userlist, hand);
  touch_laston(u, "partyline", now);
  get_user_flagrec(u, &fr, NULL);
  check_bind(BT_chon, hand, &fr, hand, idx);
}

void check_bind_chof(char *hand, int idx)
{
  struct userrec        *u = NULL;

  u = get_user_by_handle(userlist, hand);
  touch_laston(u, "partyline", now);
}

void check_bind_time(struct tm *tm)
{
	char full[32] = "";
	simple_snprintf(full, sizeof(full), "%02d %02d %02d %02d %04d", tm->tm_min, tm->tm_hour, tm->tm_mday, tm->tm_mon, tm->tm_year + 1900);
	check_bind(BT_time, full, NULL, tm->tm_min, tm->tm_hour, tm->tm_mday, tm->tm_mon, tm->tm_year + 1900);
}
