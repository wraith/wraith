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

#include "common.h"
#include "tclhash.h"
#include "debug.h"
#include "chan.h"
#include "users.h"
#include "match.h"
#include "egg_timer.h"
#include <stdarg.h>

extern struct dcc_t *dcc;
extern int dcc_total;
extern mycmds            cmdlist[];
extern int               cmdi;


/* The head of the bind table linked list. */
static bind_table_t *bind_table_list_head = NULL;

/* Garbage collection stuff. */
static int check_bind_executing = 0;
static int already_scheduled = 0;
static void bind_table_really_del(bind_table_t *table);
static void bind_entry_really_del(bind_table_t *table, bind_entry_t *entry);

void binds_init(void)
{
	bind_table_list_head = NULL;
}

static int internal_bind_cleanup()
{
	bind_table_t *table = NULL, *next_table = NULL;
	bind_entry_t *entry = NULL, *next_entry = NULL;

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
	bind_table_t *table = NULL;

	for (table = bind_table_list_head; table; table = table->next) {
		if (!strcmp(table->name, name)) break;
	}

	/* If it doesn't exist, create it. */
	if (!table) {
		table = (bind_table_t *)calloc(1, sizeof(*table));
		table->name = strdup(name);
		table->next = bind_table_list_head;
		bind_table_list_head = table;
	}
	else if (!(table->flags & BIND_FAKE)) return(table);
	table->nargs = nargs;
	if (syntax) table->syntax = strdup(syntax);
	table->match_type = match_type;
	table->flags = flags;
	return(table);
}

void bind_table_del(bind_table_t *table)
{
	bind_table_t *cur = NULL, *prev = NULL;

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
	bind_entry_t *entry = NULL, *next = NULL;

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
	bind_table_t *table = NULL;

	for (table = bind_table_list_head; table; table = table->next) {
		if (!(table->flags & BIND_DELETED) && !strcmp(table->name, name)) break;
	}
	return(table);
}

bind_table_t *bind_table_lookup_or_fake(const char *name)
{
	bind_table_t *table = NULL;

	table = bind_table_lookup(name);
	if (!table) table = bind_table_add(name, 0, NULL, 0, BIND_FAKE);
	return(table);
}


/* Look up a bind entry based on either function name or id. */
bind_entry_t *bind_entry_lookup(bind_table_t *table, int id, const char *mask, const char *function_name)
{
	bind_entry_t *entry = NULL;

	for (entry = table->entries; entry; entry = entry->next) {
		if (entry->flags & BIND_DELETED) continue;
		if (entry->id == id || (!strcmp(entry->mask, mask) && !strcmp(entry->function_name, function_name))) break;
	}
	return(entry);
}

int bind_entry_del(bind_table_t *table, int id, const char *mask, const char *function_name, void *cdata)
{
	bind_entry_t *entry = NULL;

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
	bind_entry_t *entry = NULL;

	entry = bind_entry_lookup(table, id, mask, function_name);
	if (!entry) return(-1);

	/* Modify it. */
	free(entry->mask);
	entry->mask = strdup(newmask);
	entry->user_flags.match = FR_GLOBAL | FR_CHAN;
	break_down_flags(newflags, &(entry->user_flags), NULL);

	return(0);
}

/* void blah()
{
  bind_entry_t *entry = NULL;
  bind_table_t *table = NULL;

  table = bind_table_lookup_or_fake("dcc");

  for (entry = table->entries; entry && entry->next; entry = entry->next) {
    printf("MASK: %s\n", entry->mask);
  }
}
*/

int bind_entry_add(bind_table_t *table, const char *flags, const char *mask, const char *function_name, int bind_flags, Function callback, void *client_data)
{
	bind_entry_t *entry = NULL, *old_entry = NULL;

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

/* Execute a bind entry with the given argument list. */
static int bind_entry_exec(bind_table_t *table, bind_entry_t *entry, void **al)
{
	bind_entry_t *prev = NULL;

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
	void *args[11] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	bind_entry_t *entry = NULL, *next = NULL;
	int i, cmp, retval;
	va_list ap;

	Assert(table);
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
		int matchlen, masklen, tie;
		bind_entry_t *winner;

		matchlen = strlen(match);
		tie = 0;
		winner = NULL;
		for (entry = table->entries; entry; entry = entry->next) {
			if (entry->flags & BIND_DELETED) continue;
			if (table->flags & BIND_USE_ATTR) {
				if (table->flags & BIND_STRICT_ATTR) cmp = flagrec_eq(&entry->user_flags, flags);
				else cmp = flagrec_ok(&entry->user_flags, flags);
				if (!cmp) continue;
			}
			masklen = strlen(entry->mask);
			if (!strncasecmp(match, entry->mask, masklen < matchlen ? masklen : matchlen)) {
				winner = entry;
				if (masklen == matchlen) break;
				else if (tie) return(-1);
				else tie = 1;
			}
		}
		if (winner) retval = bind_entry_exec(table, winner, args);
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

void add_builtins(const char *table_name, cmd_t *cmds)
{
	char name[50] = "";
	bind_table_t *table = NULL;

	table = bind_table_lookup_or_fake(table_name);

	for (; cmds->name; cmds++) {
                /* add BT_dcc cmds to cmdlist[] :: add to the help system.. */
                if (!strcmp(table->name, "dcc")) {
		  cmdlist[cmdi].name = cmds->name;
                  cmdlist[cmdi].flags.match = FR_GLOBAL | FR_CHAN;
                  break_down_flags(cmds->flags, &(cmdlist[cmdi].flags), NULL);
                  cmdi++;
                } 
		egg_snprintf(name, sizeof name, "*%s:%s", table->name, cmds->funcname ? cmds->funcname : cmds->name);
		bind_entry_add(table, cmds->flags, cmds->name, name, 0, cmds->func, NULL);
	}
}

void rem_builtins(const char *table_name, cmd_t *cmds)
{
	char name[50] = "";
	bind_table_t *table = NULL;

	table = bind_table_lookup(table_name);
	if (!table) return;

	for (; cmds->name; cmds++) {
		sprintf(name, "*%s:%s", table->name, cmds->funcname ? cmds->funcname : cmds->name);
		bind_entry_del(table, -1, cmds->name, name, NULL);
	}
}
