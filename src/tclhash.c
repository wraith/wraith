/*
 * tclhash.c -- handles:
 *   bind and unbind
 *   checking and triggering the various in-bot bindings
 *   listing current bindings
 *   adding/removing new binding tables
 *   (non-Tcl) procedure lookups for msg/dcc/file commands
 *   (Tcl) binding internal procedures to msg/dcc/file commands
 *
 */

#include "main.h"
#include "chan.h"
#include "users.h"
#include "match.c"
#include "egg_timer.h"

extern struct dcc_t	*dcc;
extern struct userrec	*userlist;
extern int		 dcc_total;
extern time_t		 now;
extern mycmds		 cmdlist[];
extern int		 cmdi;

extern char		dcc_prefix[];

/* New bind table list */
static bind_table_t *bind_table_list_head = NULL;
static bind_table_t *BT_link;
static bind_table_t *BT_disc;
static bind_table_t *BT_away;
static bind_table_t *BT_dcc;
static bind_table_t *BT_bot;
static bind_table_t *BT_note;
static bind_table_t *BT_nkch;
static bind_table_t *BT_chat;
static bind_table_t *BT_act;
static bind_table_t *BT_bcst;
static bind_table_t *BT_chon;
static bind_table_t *BT_chof;
static bind_table_t *BT_chpt;
static bind_table_t *BT_chjn;

/* Variables to control garbage collection. */
static int check_bind_executing = 0;
static int already_scheduled = 0;
static void bind_table_really_del(bind_table_t *table);
static void bind_entry_really_del(bind_table_t *table, bind_entry_t *entry);


extern cmd_t C_dcc[];

void binds_init(void)
{
	bind_table_list_head = NULL;
	BT_link = bind_table_add("link", 2, "ss", MATCH_MASK, BIND_STACKABLE);
        BT_nkch = bind_table_add("nkch", 2, "ss", MATCH_MASK, BIND_STACKABLE);
	BT_disc = bind_table_add("disc", 1, "s", MATCH_MASK, BIND_STACKABLE);
	BT_away = bind_table_add("away", 3, "sis", MATCH_MASK, BIND_STACKABLE);
	BT_chon = bind_table_add("chon", 2, "si", MATCH_MASK, BIND_USE_ATTR | BIND_STACKABLE);
	BT_chof = bind_table_add("chof", 2, "si", MATCH_MASK, BIND_USE_ATTR | BIND_STACKABLE);
	BT_dcc = bind_table_add("dcc", 3, "Uis", MATCH_MASK, BIND_USE_ATTR);
	BT_bot = bind_table_add("bot", 3, "sss", MATCH_EXACT, 0);
        BT_note = bind_table_add("note", 3 , "sss", MATCH_EXACT, 0);
	BT_chat = bind_table_add("chat", 3, "sis", MATCH_MASK, BIND_STACKABLE | BIND_BREAKABLE);
	BT_act = bind_table_add("act", 3, "sis", MATCH_MASK, BIND_STACKABLE);
	BT_bcst = bind_table_add("bcst", 3, "sis", MATCH_MASK, BIND_STACKABLE);
        BT_chpt = bind_table_add("chpt", 4, "ssii", MATCH_MASK, BIND_STACKABLE);
        BT_chjn = bind_table_add("chjn", 6, "ssisis", MATCH_MASK, BIND_STACKABLE);

	add_builtins("dcc", C_dcc);
}

static int internal_bind_cleanup()
{
	bind_table_t *table, *next_table;
	bind_entry_t *entry, *next_entry;

	for (table = bind_table_list_head; table; table = next_table) {
		next_table = table->next;
		if (table->flags & BIND_DELETED) {
			bind_table_really_del(table);
			continue;
		}
		for (entry = table->entries; entry; entry = next_entry) {
			next_entry = entry->next;
			if (entry->flags & BIND_DELETED) bind_entry_really_del(table, entry);
		}
	}
	already_scheduled = 0;
	return(0);
}

static void schedule_bind_cleanup()
{
	egg_timeval_t when;

	if (already_scheduled) return;
	already_scheduled = 1;

	when.sec = 0;
	when.usec = 0;
	timer_create(&when, internal_bind_cleanup);
}


void kill_binds(void)
{
	while (bind_table_list_head) bind_table_del(bind_table_list_head);
}

bind_table_t *bind_table_add(const char *name, int nargs, const char *syntax, int match_type, int flags)
{
	bind_table_t *table;

	for (table = bind_table_list_head; table; table = table->next) {
		if (!strcmp(table->name, name)) return(table);
	}
	/* Nope, we have to create a new one. */
	table = (bind_table_t *)calloc(1, sizeof(*table));
	table->name = strdup(name);
	table->nargs = nargs;
	table->syntax = strdup(syntax);
	table->match_type = match_type;
	table->flags = flags;
	table->next = bind_table_list_head;
	bind_table_list_head = table;
	return(table);
}

void bind_table_del(bind_table_t *table)
{
	bind_table_t *cur, *prev;

	for (prev = NULL, cur = bind_table_list_head; cur; prev = cur, cur = cur->next) {
		if (!strcmp(table->name, cur->name)) break;
	}

	/* If it's found, remove it from the list. */
	if (cur) {
		if (prev) prev->next = cur->next;
		else bind_table_list_head = cur->next;
	}

	/* Now delete it. */
	if (check_bind_executing) {
		table->flags |= BIND_DELETED;
		schedule_bind_cleanup();
	}
	else {
		bind_table_really_del(table);
	}
}

static void bind_table_really_del(bind_table_t *table)
{
	bind_entry_t *entry, *next;

	free(table->name);
	for (entry = table->entries; entry; entry = next) {
		next = entry->next;
		free(entry->function_name);
		free(entry->mask);
		free(entry);
	}
	free(table);
}

bind_table_t *bind_table_lookup(const char *name)
{
	bind_table_t *table;

	for (table = bind_table_list_head; table; table = table->next) {
		if (!(table->flags & BIND_DELETED) && !strcmp(table->name, name)) break;
	}
	return(table);
}

/* Look up a bind entry based on either function name or id. */
bind_entry_t *bind_entry_lookup(bind_table_t *table, int id, const char *mask, const char *function_name)
{
	bind_entry_t *entry;

	for (entry = table->entries; entry; entry = entry->next) {
		if (entry->flags & BIND_DELETED) continue;
		if (entry->id == id || (!strcmp(entry->mask, mask) && !strcmp(entry->function_name, function_name))) break;
	}
	return(entry);
}

int bind_entry_del(bind_table_t *table, int id, const char *mask, const char *function_name, void *cdata)
{
	bind_entry_t *entry;

	entry = bind_entry_lookup(table, id, mask, function_name);
	if (!entry) return(-1);


	/* Delete it. */
	if (check_bind_executing) {
		entry->flags |= BIND_DELETED;
		schedule_bind_cleanup();
	}
	else bind_entry_really_del(table, entry);
	return(0);
}

static void bind_entry_really_del(bind_table_t *table, bind_entry_t *entry)
{
	if (entry->next) entry->next->prev = entry->prev;
	if (entry->prev) entry->prev->next = entry->next;
	else table->entries = entry->next;
	free(entry->function_name);
	free(entry->mask);
	memset(entry, 0, sizeof(*entry));
	free(entry);
}

/* Modify a bind entry's flags and mask. */
int bind_entry_modify(bind_table_t *table, int id, const char *mask, const char *function_name, const char *newflags, const char *newmask)
{
	bind_entry_t *entry;

	entry = bind_entry_lookup(table, id, mask, function_name);
	if (!entry) return(-1);

	/* Modify it. */
	free(entry->mask);
	entry->mask = strdup(newmask);
	entry->user_flags.match = FR_GLOBAL | FR_CHAN;
	break_down_flags(newflags, &(entry->user_flags), NULL);

	return(0);
}

int bind_entry_add(bind_table_t *table, const char *flags, const char *mask, const char *function_name, int bind_flags, Function callback, void *client_data)
{
	bind_entry_t *entry, *old_entry;

	old_entry = bind_entry_lookup(table, -1, mask, function_name);

	if (old_entry) {
		if (table->flags & BIND_STACKABLE) {
			entry = (bind_entry_t *)calloc(1, sizeof(*entry));
			entry->prev = old_entry;
			entry->next = old_entry->next;
			old_entry->next = entry;
			if (entry->next) entry->next->prev = entry;
		}
		else {
			entry = old_entry;
			free(entry->function_name);
			free(entry->mask);
		}
	}
	else {
		for (old_entry = table->entries; old_entry && old_entry->next; old_entry = old_entry->next) {
			; /* empty loop */
		}
		entry = (bind_entry_t *)calloc(1, sizeof(*entry));
		if (old_entry) old_entry->next = entry;
		else table->entries = entry;
		entry->prev = old_entry;
	}

	entry->mask = strdup(mask);
	entry->function_name = strdup(function_name);
	entry->callback = callback;
	entry->client_data = client_data;
	entry->flags = bind_flags;

	entry->user_flags.match = FR_GLOBAL | FR_CHAN;
	break_down_flags(flags, &(entry->user_flags), NULL);

	return(0);
}

int findanyidx(register int z)
{
  register int j;

  for (j = 0; j < dcc_total; j++)
    if (dcc[j].sock == z)
      return j;
  return -1;
}

/* Execute a bind entry with the given argument list. */
static int bind_entry_exec(bind_table_t *table, bind_entry_t *entry, void **al)
{
	bind_entry_t *prev;

	/* Give this entry a hit. */
	entry->nhits++;

	/* Search for the last entry that isn't deleted. */
	for (prev = entry->prev; prev; prev = prev->prev) {
		if (!(prev->flags & BIND_DELETED) && (prev->nhits >= entry->nhits)) break;
	}

	/* See if this entry is more popular than the preceding one. */
	if (entry->prev != prev) {
		/* Remove entry. */
		if (entry->prev) entry->prev->next = entry->next;
		else table->entries = entry->next;
		if (entry->next) entry->next->prev = entry->prev;

		/* Re-add in correct position. */
		if (prev) {
			entry->next = prev->next;
			if (prev->next) prev->next->prev = entry;
			prev->next = entry;
		}

		else {
			entry->next = table->entries;
			table->entries = entry;
		}
		entry->prev = prev;
		if (entry->next) entry->next->prev = entry;
	}

	/* Does the callback want client data? */
	if (entry->flags & BIND_WANTS_CD) {
		*al = entry->client_data;
	}
	else al++;

	return entry->callback(al[0], al[1], al[2], al[3], al[4], al[5], al[6], al[7], al[8], al[9]);
}		

int check_bind(bind_table_t *table, const char *match, struct flag_record *flags, ...)
{
	void *args[11];
	bind_entry_t *entry, *next;
	int i, cmp, retval;
	va_list ap;

	check_bind_executing++;

	va_start(ap, flags);
	for (i = 1; i <= table->nargs; i++) {
		args[i] = va_arg(ap, void *);
	}
	va_end(ap);

	/* Default return value is 0 */
	retval = 0;

	/* If it's a partial bind, we have to find the closest match. */
	if (table->match_type & MATCH_PARTIAL) {
		int len, tie;

		len = strlen(match);
		tie = 0;
		for (entry = table->entries; entry; entry = entry->next) {
			if (entry->flags & BIND_DELETED) continue;
			if (!strncasecmp(match, entry->mask, len)) {
				if (tie) return(-1);
				tie = 1;
			}
		}
		if (entry && !tie) retval = bind_entry_exec(table, entry, args);
		else retval = -1;
		check_bind_executing--;
		return(retval);
	}

	for (entry = table->entries; entry; entry = next) {
		next = entry->next;
		if (entry->flags & BIND_DELETED) continue;

		if (table->match_type & MATCH_MASK) {
			cmp = !wild_match_per((unsigned char *)entry->mask, (unsigned char *)match);
		}
		else {
			if (table->match_type & MATCH_CASE) cmp = strcmp(entry->mask, match);
			else cmp = strcasecmp(entry->mask, match);
		}
		if (cmp) continue; /* Doesn't match. */

		/* Check flags. */
		if (table->flags & BIND_USE_ATTR) {
			if (table->flags & BIND_STRICT_ATTR) cmp = flagrec_eq(&entry->user_flags, flags);
			else cmp = flagrec_ok(&entry->user_flags, flags);
			if (!cmp) continue;
		}

		retval = bind_entry_exec(table, entry, args);
		if ((table->flags & BIND_BREAKABLE) && (retval & BIND_RET_BREAK)) {
			check_bind_executing--;
			return(retval);
		}
	}
	check_bind_executing--;
	return(retval);
}

/* Check for tcl-bound dcc command, return 1 if found
 * dcc: proc-name <handle> <sock> <args...>
 */
void check_dcc(const char *cmd, int idx, const char *text)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  int x;
#ifdef S_DCCPASS
  int found = 0;
#endif

  get_user_flagrec(dcc[idx].user, &fr, dcc[idx].u.chat->con_chan);

#ifdef S_DCCPASS
  for (hm = H_dcc->first; hm; hm = hm->next) {
    if (!egg_strcasecmp(cmd, hm->mask)) {
      found = 1;
      break;
    }
  }
  if (found) {
    if (has_cmd_pass(cmd)) {
      char *p,
        work[1024],
        pass[128];
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
        return 0;
      }
    }
  }
#endif /* S_DCCPASS */
  x = check_bind(BT_dcc, cmd, &fr, dcc[idx].user, idx, text);
  if (x == 0)
    dprintf(idx, "What?  You need '%shelp'\n", dcc_prefix);
  else if (x & BIND_RET_LOG) {
     putlog(LOG_CMDS, "*", "#%s# %s %s", dcc[idx].nick, cmd, text);
  }
}

void check_bot(const char *nick, const char *code, const char *param)
{
  check_bind(BT_bot, code, NULL, nick, code, param);
}

void check_chon(char *hand, int idx)
{
  struct flag_record     fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  struct userrec        *u;

  u = get_user_by_handle(userlist, hand);
  touch_laston(u, "partyline", now);
  get_user_flagrec(u, &fr, NULL);
  check_bind(BT_chon, hand, &fr, hand, idx);
}

void check_chof(char *hand, int idx)
{
  struct flag_record     fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  struct userrec        *u;

  u = get_user_by_handle(userlist, hand);
  touch_laston(u, "partyline", now);
  get_user_flagrec(u, &fr, NULL);
  check_bind(BT_chof, hand, &fr, hand, idx);
}

int check_chat(char *handle, int chan, const char *text)
{
  return check_bind(BT_chat, text, NULL, handle, chan, text);
}

void check_act(const char *from, int chan, const char *text)
{
  check_bind(BT_act, text, NULL, from, chan, text);
}

void check_bcst(const char *from, int chan, const char *text)
{
  check_bind(BT_bcst, text, NULL, from, chan, text);
}

void check_nkch(const char *ohand, const char *nhand)
{
  check_bind(BT_nkch, ohand, NULL, ohand, nhand);
}

void check_link(const char *bot, const char *via)
{
  check_bind(BT_link, bot, NULL, bot, via);
}

void check_disc(const char *bot)
{
  check_bind(BT_disc, bot, NULL, bot);
}

int check_note(const char *from, const char *to, const char *text)
{
  return check_bind(BT_note, to, NULL, from, to, text);
}

void check_chjn(const char *bot, const char *nick, int chan,
                    const char type, int sock, const char *host)
{
  struct flag_record    fr = {FR_GLOBAL, 0, 0, 0, 0, 0};
  char                  s[11], t[2];

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

void check_chpt(const char *bot, const char *hand, int sock, int chan)
{
  char  v[11];

  egg_snprintf(v, sizeof v, "%d", chan);
  check_bind(BT_chpt, v, NULL, bot, hand, sock, chan);
}

void check_away(const char *bot, int idx, const char *msg)
{
  check_bind(BT_away, bot, NULL, bot, idx, msg);
}

void add_builtins(const char *table_name, cmd_t *cmds)
{
	char name[50];
	bind_table_t *table;

	table = bind_table_lookup(table_name);
	if (!table) return;

	for (; cmds->name; cmds++) {
                /* add BT_dcc cmds to cmdlist[] :: add to the help system.. */
                if (!strcmp(table->name, "dcc") && !(cmds->nohelp)) {
		  cmdlist[cmdi].name = cmds->name;
                  cmdlist[cmdi].flags.match = FR_GLOBAL | FR_CHAN;
                  break_down_flags(cmds->flags, &(cmdlist[cmdi].flags), NULL);
                  cmdi++;
                }
		egg_snprintf(name, 50, "*%s:%s", table->name, cmds->funcname ? cmds->funcname : cmds->name);
		bind_entry_add(table, cmds->flags, cmds->name, name, 0, cmds->func, NULL);
	}
}

void rem_builtins(const char *table_name, cmd_t *cmds)
{
	char name[50];
	bind_table_t *table;

	table = bind_table_lookup(table_name);
	if (!table) return;

	for (; cmds->name; cmds++) {
		sprintf(name, "*%s:%s", table->name, cmds->funcname ? cmds->funcname : cmds->name);
		bind_entry_del(table, -1, cmds->name, name, NULL);
	}
}
