/*
 * core_binds.c -- handles:
 *
 *   binds for the CORE
 *
 */

#include "common.h"
#include "dccutil.h"
#include "userrec.h"
#include "main.h"
#include "settings.h"
#include "users.h"
#include "misc.h"
#include "tclhash.h"
#ifdef S_DCCPASS
#include "cfg.h"
#endif /* S_DCCPASS */

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
        BT_dcc = bind_table_add("dcc", 3, "Uis", MATCH_PARTIAL | MATCH_FLAGS, 0);
        add_builtins("dcc", C_dcc);
        BT_nkch = bind_table_add("nkch", 2, "ss", MATCH_MASK, BIND_STACKABLE);
        BT_note = bind_table_add("note", 3 , "sss", MATCH_EXACT, 0);
	BT_time = bind_table_add("time", 5, "iiiii", MATCH_MASK, BIND_STACKABLE);
}

void check_bind_dcc(const char *cmd, int idx, const char *text)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  int x, hits;
#ifdef S_DCCPASS
  bind_entry_t *entry = NULL;
  bind_table_t *table = NULL;
  int found = 0;
  char *args = strdup(text);
#endif

  get_user_flagrec(dcc[idx].user, &fr, dcc[idx].u.chat->con_chan);

#ifdef S_DCCPASS
  table = bind_table_lookup("dcc");
  for (entry = table->entries; entry && entry->next; entry = entry->next) {
    if (!egg_strcasecmp(cmd, entry->mask)) {
      found = 1;
      break;
    }
  }

  if (found) {
    if (has_cmd_pass(cmd)) {
      char *p = NULL, work[1024] = "", pass[128] = "";

      p = strchr(args, ' ');
      if (p)
        *p = 0;
      strncpyz(pass, args, sizeof(pass));

      if (check_cmd_pass(cmd, pass)) {
        if (p)
          *p = ' ';
        strncpyz(work, args, sizeof(work));
        p = work;
        newsplit(&p);
        strcpy(args, p);
      } else {
        dprintf(idx, "Invalid command password. Use %scommand password arguments\n", dcc_prefix);
        putlog(LOG_MISC, "*", "%s attempted %s%s with missing or incorrect command password", dcc[idx].nick, dcc_prefix, cmd);
        free(args);
        return;
      }
    }
  }
#endif /* S_DCCPASS */

  x = check_bind_hits(BT_dcc, cmd, &fr, &hits, dcc[idx].user, idx, args);

  if (hits != 1)
    putlog(LOG_CMDS, "*", "! #%s# %s %s", dcc[idx].nick, cmd, args);

  if (hits == 0)
    dprintf(idx, "What?  You need '%shelp'\n", dcc_prefix);
  else if (hits > 1)
    dprintf(idx, "Ambiguous command.\n");

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

