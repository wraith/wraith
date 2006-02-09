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
#include "tclhash.h"
#include "dcc.h"

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
	egg_bzero(&cmdlist, 500);
        add_builtins("dcc", C_dcc);
        BT_nkch = bind_table_add("nkch", 2, "ss", MATCH_MASK, BIND_STACKABLE);
        BT_note = bind_table_add("note", 3 , "sss", MATCH_EXACT, 0);
	BT_time = bind_table_add("time", 5, "iiiii", MATCH_MASK, BIND_STACKABLE);
}

bool check_aliases(int idx, const char *cmd, const char *args)
{
  char *a = NULL, *p = NULL, *aliasp = NULL, *aliasdup = NULL, *argsp = NULL, *argsdup = NULL;
  bool found = 0;

  aliasp = aliasdup = strdup(alias);
  argsp = argsdup = strdup(args);

  while ((a = strsep(&aliasdup, ","))) { //a = entire alias "alias cmd params"
    p = newsplit(&a);   //p = alias cmd //a = rcmd params
    if (!egg_strcasecmp(p, cmd)) { //a match on the cmd we were given!
      p = newsplit(&a); //p = rcmd //a = params

      if (!egg_strcasecmp(cmd, p)) {
        putlog(LOG_WARN, "*", "Loop detected in alias '%s'", p);
        free(argsp);
        return 0;
      }

      char *myargs = NULL, *pass = NULL;
      size_t size = 0;

      found = 1;

      size = strlen(a) + 1 + strlen(argsdup) + 1 + 2;
      myargs = (char *) calloc(1, size);

      if (has_cmd_pass(p)) {
        pass = newsplit(&argsdup);
        simple_snprintf(myargs, size, "%s %s %s", pass, a, argsdup);
      } else
        simple_snprintf(myargs, size, "%s %s", a, argsdup);
      putlog(LOG_CMDS, "*", "@ #%s# [%s -> %s %s] ...", dcc[idx].nick, cmd, p, a);
      check_bind_dcc(p, idx, myargs);

      if (myargs)
        free(myargs);
      break;
    }
  }

  free(aliasp);
  free(argsp);

  return found;
}

void check_bind_dcc(const char *cmd, int idx, const char *text)
{
  real_check_bind_dcc(cmd, idx, text, NULL);
}

void real_check_bind_dcc(const char *cmd, int idx, const char *text, Auth *auth)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  bind_entry_t *entry = NULL;
  bind_table_t *table = NULL;
  char *args = strdup(text);

  get_user_flagrec(dcc[idx].user, &fr, dcc[idx].u.chat->con_chan);

  table = bind_table_lookup("dcc");
  size_t cmdlen = strlen(cmd);

  for (entry = table->entries; entry && entry->next; entry = entry->next) {
    if (!egg_strncasecmp(cmd, entry->mask, cmdlen)) {
      if (has_cmd_pass(entry->mask)) {
        if (flagrec_ok(&entry->user_flags, &fr)) {
          char *p = NULL, work[1024] = "", pass[128] = "";

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
            strcpy(args, p);
          } else {
            dprintf(idx, "Invalid command password.\n");
            dprintf(idx, "Use: $b%scommand <password> [arguments]$b\n", settings.dcc_prefix);
            if (p)
              p++;
            putlog(LOG_CMDS, "*", "$ #%s# %s **hidden** %s", dcc[idx].nick, entry->mask, p && *p ? p : "");
            putlog(LOG_MISC, "*", "%s attempted %s%s with missing or incorrect command password", dcc[idx].nick, settings.dcc_prefix, entry->mask);
            free(args);
            return;
          }
        } else {
          putlog(LOG_CMDS, "*", "! #%s# %s %s", dcc[idx].nick, cmd, args);
          dprintf(idx, "What?  You need '%shelp'\n", settings.dcc_prefix);
          free(args);
          return;
        }
      }
      break;
    }
  }

  if (entry && auth) {
    if (!(entry->cflags & AUTH))
      return;
  }

  int hits = 0;
  bool log_bad = 0;

  check_bind_hits(BT_dcc, cmd, &fr, &hits, idx, args);

  if (hits != 1)
    log_bad = 1;

  if (hits == 0) {
    if (!check_aliases(idx, cmd, args)) 
      dprintf(idx, "What?  You need '%shelp'\n", settings.dcc_prefix);
    else
      log_bad = 0;
  } else if (hits > 1)
    dprintf(idx, "Ambiguous command.\n");

  if (log_bad)
    putlog(LOG_CMDS, "*", "! #%s# %s %s", dcc[idx].nick, cmd, args);

  free(args);
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

void check_bind_nkch(const char *ohand, const char *nhand)
{
  check_bind(BT_nkch, ohand, NULL, ohand, nhand);
}

int check_bind_note(const char *from, const char *to, const char *text)
{
  return check_bind(BT_note, to, NULL, from, to, text);
}

void check_bind_away(const char *bot, int idx, const char *msg)
{
  check_bind(BT_away, bot, NULL, bot, idx, msg);
}

void check_bind_time(struct tm *tm)
{
	char full[32] = "";
	egg_snprintf(full, sizeof(full), "%02d %02d %02d %02d %04d", tm->tm_min, tm->tm_hour, tm->tm_mday, tm->tm_mon, tm->tm_year + 1900);
	check_bind(BT_time, full, NULL, tm->tm_min, tm->tm_hour, tm->tm_mday, tm->tm_mon, tm->tm_year + 1900);
}

