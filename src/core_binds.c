/*
 * core_binds.c -- handles:
 *
 *   binds for the CORE
 *
 */

#include "common.h"
#include "dccutil.h"
#include "userrec.h"
#include "users.h"
#include "misc.h"
#include "tclhash.h"
#ifdef S_DCCPASS
#include "cfg.h"
#endif /* S_DCCPASS */

extern cmd_t 		C_dcc[];
extern struct dcc_t 	*dcc;
extern struct userrec 	*userlist;
extern time_t 		now;
extern char             dcc_prefix[];



static bind_table_t *BT_link = NULL, *BT_disc = NULL, *BT_away = NULL, *BT_dcc = NULL;
static bind_table_t *BT_chat = NULL, *BT_act = NULL, *BT_bcst = NULL, *BT_note = NULL;
static bind_table_t *BT_bot = NULL, *BT_nkch = NULL, *BT_chon = NULL, *BT_chof = NULL;
static bind_table_t *BT_chpt = NULL, *BT_chjn = NULL, *BT_time = NULL, *BT_event = NULL;

void core_binds_init()
{
        BT_act = bind_table_add("act", 3, "sis", MATCH_MASK, BIND_STACKABLE);
        BT_away = bind_table_add("away", 3, "sis", MATCH_MASK, BIND_STACKABLE);
        BT_bcst = bind_table_add("bcst", 3, "sis", MATCH_MASK, BIND_STACKABLE);
        BT_bot = bind_table_add("bot", 3, "sss", MATCH_EXACT, 0);
        BT_chat = bind_table_add("chat", 3, "sis", MATCH_MASK, BIND_STACKABLE | BIND_BREAKABLE);
        BT_chjn = bind_table_add("chjn", 6, "ssisis", MATCH_MASK, BIND_STACKABLE);
        BT_chon = bind_table_add("chon", 2, "si", MATCH_MASK, BIND_USE_ATTR | BIND_STACKABLE);
        BT_chof = bind_table_add("chof", 2, "si", MATCH_MASK, BIND_USE_ATTR | BIND_STACKABLE);
        BT_chpt = bind_table_add("chpt", 4, "ssii", MATCH_MASK, BIND_STACKABLE);
        BT_dcc = bind_table_add("dcc", 3, "Uis", MATCH_PARTIAL, BIND_USE_ATTR);
        add_builtins("dcc", C_dcc);
        BT_disc = bind_table_add("disc", 1, "s", MATCH_MASK, BIND_STACKABLE);
	BT_event = bind_table_add("event", 1, "s", MATCH_MASK, BIND_STACKABLE);
        BT_link = bind_table_add("link", 2, "ss", MATCH_MASK, BIND_STACKABLE);
        BT_nkch = bind_table_add("nkch", 2, "ss", MATCH_MASK, BIND_STACKABLE);
        BT_note = bind_table_add("note", 3 , "sss", MATCH_EXACT, 0);
	BT_time = bind_table_add("time", 5, "iiiii", MATCH_MASK, BIND_STACKABLE);


}

void check_bind_dcc(const char *cmd, int idx, const char *text)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  int x;
#ifdef S_DCCPASS
  bind_entry_t *entry = NULL;
  bind_table_t *table = NULL;
  int found = 0;
  char *args = NULL;
#endif

  get_user_flagrec(dcc[idx].user, &fr, dcc[idx].u.chat->con_chan);

#ifdef S_DCCPASS
  args = strdup(text);

  table = bind_table_lookup("dcc");
  for (entry = table->entries; entry && entry->next; entry = entry->next) {
    if (!egg_strcasecmp(cmd, entry->mask)) {
      found = 1;
      break;
    }
  }

  if (found) {
    if (has_cmd_pass(cmd)) {
      char *p = NULL, pass[128] = "";

      p = strchr(args, ' ');
      if (p)
        *p = 0;
      strncpyz(pass, args, sizeof(pass));

      if (!check_cmd_pass(cmd, pass)) {
        dprintf(idx, "Invalid command password. Use %scommand password arguments\n", dcc_prefix);
        putlog(LOG_MISC, "*", "%s attempted %s%s with missing or incorrect command password", dcc[idx].nick, dcc_prefix, cmd);
        free(args);
        return;
      }
    }
  }
  free(args);
#endif /* S_DCCPASS */

  x = check_bind(BT_dcc, cmd, &fr, dcc[idx].user, idx, text);
  putlog(LOG_DEBUG, "*", "%s RETURNED: %d", cmd, x);
  if (x == -1)
    dprintf(idx, "What?  You need '%shelp'\n", dcc_prefix);
  else if (x & BIND_RET_LOG) {
     putlog(LOG_CMDS, "*", "#%s# %s %s", dcc[idx].nick, cmd, text);
  }
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
  struct flag_record     fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  struct userrec        *u = NULL;

  u = get_user_by_handle(userlist, hand);
  touch_laston(u, "partyline", now);
  get_user_flagrec(u, &fr, NULL);
  check_bind(BT_chon, hand, &fr, hand, idx);
}

void check_bind_chof(char *hand, int idx)
{
  struct flag_record     fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  struct userrec        *u = NULL;

  u = get_user_by_handle(userlist, hand);
  touch_laston(u, "partyline", now);
  get_user_flagrec(u, &fr, NULL);
  check_bind(BT_chof, hand, &fr, hand, idx);
}

int check_bind_chat(char *handle, int chan, const char *text)
{
  return check_bind(BT_chat, text, NULL, handle, chan, text);
}

void check_bind_act(const char *from, int chan, const char *text)
{
  check_bind(BT_act, text, NULL, from, chan, text);
}

void check_bind_bcst(const char *from, int chan, const char *text)
{
  check_bind(BT_bcst, text, NULL, from, chan, text);
}

void check_bind_nkch(const char *ohand, const char *nhand)
{
  check_bind(BT_nkch, ohand, NULL, ohand, nhand);
}

void check_bind_link(const char *bot, const char *via)
{
  check_bind(BT_link, bot, NULL, bot, via);
}

void check_bind_disc(const char *bot)
{
  check_bind(BT_disc, bot, NULL, bot);
}

int check_bind_note(const char *from, const char *to, const char *text)
{
  return check_bind(BT_note, to, NULL, from, to, text);
}

void check_bind_chjn(const char *bot, const char *nick, int chan,
                    const char type, int sock, const char *host)
{
  struct flag_record    fr = {FR_GLOBAL, 0, 0, 0, 0, 0};
  char                  s[11] = "", t[2] = "";

  t[0] = type;
  t[1] = 0;
  switch (type) {
  case '^':
    fr.global = USER_ADMIN;
    break;
  case '*':
    fr.global = USER_OWNER;
    break;
  case '+':
    fr.global = USER_MASTER;
    break;
  case '@':
    fr.global = USER_OP;
    break;
  }
  egg_snprintf(s, sizeof s, "%d", chan);
  check_bind(BT_chjn, s, &fr, bot, nick, chan, t, sock, host);
}

void check_bind_chpt(const char *bot, const char *hand, int sock, int chan)
{
  char  v[11] = "";

  egg_snprintf(v, sizeof v, "%d", chan);
  check_bind(BT_chpt, v, NULL, bot, hand, sock, chan);
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

void check_bind_event(char *event)
{
	check_bind(BT_event, event, NULL, event);
}
