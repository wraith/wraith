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


static char *my_strdup(const char *s)
{
	char *t;

	t = (char *)malloc(strlen(s)+1);
	strcpy(t, s);
	return(t);
}


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
	table = (bind_table_t *)malloc(sizeof(*table));
	table->chains = NULL;
	table->name = my_strdup(name);
	table->nargs = nargs;
	table->syntax = my_strdup(syntax);
	table->match_type = match_type;
	table->flags = flags;
	table->next = bind_table_list_head;
	bind_table_list_head = table;
	return(table);
}

void bind_table_del(bind_table_t *table)
{
	bind_table_t *cur, *prev;
	bind_chain_t *chain, *next_chain;
	bind_entry_t *entry, *next_entry;

	for (prev = NULL, cur = bind_table_list_head; cur; prev = cur, cur = cur->next) {
		if (!strcmp(table->name, cur->name)) break;
	}

	/* If it's found, remove it from the list. */
	if (cur) {
		if (prev) prev->next = cur->next;
		else bind_table_list_head = cur->next;
	}

	/* Now delete it. */
	free(table->name);
	for (chain = table->chains; chain; chain = next_chain) {
		next_chain = chain->next;
		for (entry = chain->entries; entry; entry = next_entry) {
			next_entry = entry->next;
			free(entry->function_name);
			free(entry);
		}
		free(chain);
	}
}

bind_table_t *bind_table_find(const char *name)
{
	bind_table_t *table;

	for (table = bind_table_list_head; table; table = table->next) {
		if (!strcmp(table->name, name)) break;
	}
	return(table);
}

int bind_entry_del(bind_table_t *table, const char *flags, const char *mask, const char *function_name, void *cdata)
{
	bind_chain_t *chain;
	bind_entry_t *entry, *prev;

	/* Find the correct mask entry. */
	for (chain = table->chains; chain; chain = chain->next) {
		if (!strcmp(chain->mask, mask)) break;
	}
	if (!chain) return(1);

	/* Now find the function name in this mask entry. */
	for (prev = NULL, entry = chain->entries; entry; prev = entry, entry = entry->next) {
		if (!strcmp(entry->function_name, function_name)) break;
	}
	if (!entry) return(1);

	/* Delete it. */
	if (prev) prev->next = entry->next;
	else chain->entries = entry->next;
	free(entry->function_name);
	free(entry);

	return(0);
}

int bind_entry_add(bind_table_t *table, const char *flags, const char *mask, const char *function_name, int bind_flags, Function callback, void *client_data)
{
	bind_chain_t *chain;
	bind_entry_t *entry;

	/* Find the chain (mask) first. */
	for (chain = table->chains; chain; chain = chain->next) {
		if (!strcmp(chain->mask, mask)) break;
	}

	/* Create if it doesn't exist. */
	if (!chain) {
		chain = (bind_chain_t *)malloc(sizeof(*chain));
		chain->entries = NULL;
		chain->mask = my_strdup(mask);
		chain->next = table->chains;
		table->chains = chain;
	}

	/* If it's stackable */
	if (table->flags & BIND_STACKABLE) {
		/* Search for specific entry. */
		for (entry = chain->entries; entry; entry = entry->next) {
			if (!strcmp(entry->function_name, function_name)) break;
		}
	}
	else {
		/* Nope, just use first entry. */
		entry = chain->entries;
	}

	/* If we have an old entry, re-use it. */
	if (entry) free(entry->function_name);
	else {
		/* Otherwise, create a new entry. */
		entry = (bind_entry_t *)malloc(sizeof(*entry));
		entry->next = chain->entries;
		chain->entries = entry;
	}

	entry->function_name = my_strdup(function_name);
	entry->callback = callback;
	entry->client_data = client_data;
	entry->hits = 0;
	entry->bind_flags = bind_flags;

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

int check_bind(bind_table_t *table, const char *match, struct flag_record *_flags, ...)
{
	int *al; /* Argument list */
	struct flag_record *flags;
	bind_chain_t *chain;
	bind_entry_t *entry;
	int len, cmp, r, hits;
	Function cb;

	/* Experimental way to not use va_list... */
	flags = _flags;
	al = (int *)&_flags;

	/* Save the length for strncmp */
	len = strlen(match);

	/* Keep track of how many binds execute (or would) */
	hits = 0;

	/* Default return value is 0 */
	r = 0;

	/* For each chain in the table... */
	for (chain = table->chains; chain; chain = chain->next) {
		/* Test to see if it matches. */
		if (table->match_type & MATCH_PARTIAL) {
			if (table->match_type & MATCH_CASE) cmp = strncmp(match, chain->mask, len);
			else cmp = egg_strncasecmp(match, chain->mask, len);
		}
		else if (table->match_type & MATCH_MASK) {
			cmp = !wild_match_per((unsigned char *)chain->mask, (unsigned char *)match);
		}
		else {
			if (table->match_type & MATCH_CASE) cmp = strcmp(match, chain->mask);
			else cmp = egg_strcasecmp(match, chain->mask);
		}
		if (cmp) continue; /* Doesn't match. */

		/* Ok, now call the entries for this chain. */
		/* If it's not stackable, There Can Be Only One. */
		for (entry = chain->entries; entry; entry = entry->next) {
			/* Check flags. */
			if (table->match_type & BIND_USE_ATTR) {
				if (table->match_type & BIND_STRICT_ATTR) cmp = flagrec_eq(&entry->user_flags, flags);
				else cmp = flagrec_ok(&entry->user_flags, flags);
				if (!cmp) continue;
			}

			/* This is a hit */
			hits++;
		
			cb = entry->callback;
			if (entry->bind_flags & BIND_WANTS_CD) {
				al[0] = (int) entry->client_data;
				table->nargs++;
			}
			else al++;

			/* Default return value is 0. */
			r = 0;

			switch (table->nargs) {
				case 0: r = cb(); break;
				case 1: r = cb(al[0]); break;
				case 2: r = cb(al[0], al[1]); break;
				case 3: r = cb(al[0], al[1], al[2]); break;
				case 4: r = cb(al[0], al[1], al[2], al[3]); break;
				case 5: r = cb(al[0], al[1], al[2], al[3], al[4]); break;
				case 6: r = cb(al[0], al[1], al[2], al[3], al[4], al[5]); break;
				case 7: r = cb(al[0], al[1], al[2], al[3], al[4], al[5], al[6]);
				case 8: r = cb(al[0], al[1], al[2], al[3], al[4], al[5], al[6], al[7]);
			}

			if (entry->bind_flags & BIND_WANTS_CD) table->nargs--;
			else al--;

			if ((table->flags & BIND_BREAKABLE) && (r & BIND_RET_BREAK)) return(r);
		}
	}
	return(r);
}

/* Check for tcl-bound dcc command, return 1 if found
 * dcc: proc-name <handle> <sock> <args...>
 */
void check_dcc(const char *cmd, int idx, const char *args)
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
//    dprintf(idx, "What?  You need '%shelp'\n", dcc_prefix);
  x = check_bind(BT_dcc, cmd, &fr, dcc[idx].user, idx, args);
  if (x & BIND_RET_LOG) {
     putlog(LOG_CMDS, "*", "#%s# %s %s", dcc[idx].nick, cmd, args);
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

	table = bind_table_find(table_name);
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

	table = bind_table_find(table_name);
	if (!table) return;

	for (; cmds->name; cmds++) {
		sprintf(name, "*%s:%s", table->name, cmds->funcname ? cmds->funcname : cmds->name);
		bind_entry_del(table, cmds->flags, cmds->name, name, NULL);
	}
}
