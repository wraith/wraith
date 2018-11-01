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
 * binds.c -- handles:
 *   bind and unbind
 *   checking and triggering the various in-bot bindings
 *   listing current bindings
 *   adding/removing new binding tables
 *   (non-Tcl) procedure lookups for msg/dcc/file commands
 *   (Tcl) binding internal procedures to msg/dcc/file commands
 *
 */


#include "common.h"
#include "binds.h"
#include "cmds.h"
#include "debug.h"
#include "chan.h"
#include "users.h"
#include "match.h"
#include "egg_timer.h"
#include <stdarg.h>


/* The head of the bind table linked list. */
static bind_table_t *bind_table_list_head = NULL;

/* Garbage collection stuff. */
static int check_bind_executing = 0;
static int already_scheduled = 0;
/* main routine for bind checks */
static int bind_vcheck_hits (bind_table_t *table, const char *match, struct flag_record *flags, int *hits, va_list args);
static void bind_table_really_del(bind_table_t *table);
static void bind_entry_really_del(bind_table_t *table, bind_entry_t *entry);

static const char *maze = ".===============================================================.\n|  ,-----------------,                                          |\n| /| HELP THE BUNNY  |==========.===.   .===.   .===========.   |\n|| |    FIND HIS     |          |   |   |   |   |      .-.  | E |\n|| |  EASTER EGGS!   |  |===|   |   |   |   |   |..==./xxx\\ | N |\n|| |_________________|          |   |       |   /<<<<<\\    || D |\n||/_________________/   .======='   |   .   |   \\>>>>>/xxxx/--. |\n|   |   |           |   |           |   |   |   |`'==''---; * *`\\\n|   |   '==========='   |   |======='   |   |   |   ,===. \\* * */\n|   |                   |               |   |   |   |   |  '--'`|\n|   '===============|   '===.   |===.   |   |   |==='   '=======|\n|                           |       |   |   |   |               |\n|   |===============.   .   '===|   |   |===|   |   .=======.   |\n|                   |   |           |   |   |   |   |       |   |\n|   .===========.   |   |   |===.   |   |   |   |   |   |   |   |\n|   |           |   |   |       |   |   |   |   |   |   |   |   |\n|   |   .===.   |   |   |===.   '===|   |   '==='   |   |   |   |\n|   |   |   |   |   |   |   |       |   |           |   |   |   |\n|   '==='   /`\\ '==='   |   '===.   |   '===========|   |   |   |\n|          / : |                |   |               |   |   |   |\n| _.._=====| '/ '===|   .======='   '===========.   |   |   |   |\n  .-._ '-\"` (=======.   |   .===============.   |   |   '===.   |\n_/  |/   e  e\\==.   |   |   |               |   |   |       |   |\n| S ||  >   @ )<|   |   |   |   .=======.   |   |   |===.   |   |\n| T | \\  '--`/  |   |   |   |   |       |   |   |   |   |   |   |\n| A | / '--<`   |   |   |   |   |   |   |   |   '==='   |   '   |\n| R || ,    \\\\  |           |   |   |   |   |           |       |\n| T |; ;     \\\\__'======.   |   |   '==='   |   .===.   |   |   |\n|   / /      |.__)==,   |   |   |           |   |   |   |   |   |\n|  (_/,--.   ; //\"\"\"\\\\  |   |   '==========='   |   '==='   |   |\n|  { `|   \\_/  ||___||  |   |                   |           |   |\n|   ;-\\   / |  |(___)|  |   '===========.   |   '=======.   |   |\n|   |  | /  |  |XXXXX|  |               |   |           |   |   |\n|   | /  \\  '-,\\XXXXX/  |   .==========='   '=======.   |   |   |\n|   | \\__|----' `\"\"\"`   |   |                       |   |   |   |\n|   '==================='   '======================='   '==='   |\n|                                                               |\n'==============================================================='";

static int internal_bind_cleanup()
{
	bind_table_t *table = NULL, *next_table = NULL;
	bind_entry_t *entry = NULL, *next_entry = NULL;

	if (maze) { ; } /* gcc warnings */

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
	if (already_scheduled)
          return;

	already_scheduled = 1;

	egg_timeval_t when = { 0, 0 };

	timer_create(&when, "internal_bind_cleanup", internal_bind_cleanup);
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
		table = (bind_table_t *) calloc(1, sizeof(*table));
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

	for (entry = table->entries; entry; entry = next) {
		next = entry->next;
		if (entry->function_name) free(entry->function_name);
		if (entry->mask) free(entry->mask);
		free(entry);
	}
	if (table->name) free(table->name);
	if (table->syntax) free(table->syntax);
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
static bind_entry_t *bind_entry_lookup(bind_table_t *table, int id, const char *mask, const char *function_name, Function callback)
{
	bind_entry_t *entry = NULL;
	int hit;

	for (entry = table->entries; entry; entry = entry->next) {
		if (entry->flags & BIND_DELETED) continue;
		if (entry->id >= 0) {
			if (entry->id == id) break;
		}
		else {
			hit = 0;
			if (entry->mask && !strcmp(entry->mask, mask)) hit++;
			else if (!entry->mask) hit++;
			if (entry->function_name && !strcmp(entry->function_name, function_name)) hit++;
			if (entry->callback == (HashFunc) callback || !callback) hit++;
			if (hit == 3) break;
		}
	}
	return(entry);
}

static int bind_entry_del(bind_table_t *table, int id, const char *mask, const char *function_name, Function callback)
{
	bind_entry_t *entry = NULL;

	entry = bind_entry_lookup(table, id, mask, function_name, callback);
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
	if (entry->function_name) free(entry->function_name);
	if (entry->mask) free(entry->mask);
	memset(entry, 0, sizeof(*entry));
	free(entry);
}

int bind_entry_add(bind_table_t *table, const char *flags, int cflags, const char *mask, const char *function_name, int bind_flags, Function callback, void *client_data)
{
	bind_entry_t *entry = NULL, *old_entry = bind_entry_lookup(table, -1, mask, function_name, NULL);

	if (old_entry) {
		if (table->flags & BIND_STACKABLE) {
			entry = (bind_entry_t *) calloc(1, sizeof(*entry));
			entry->prev = old_entry;
			entry->next = old_entry->next;
			old_entry->next = entry;
			if (entry->next) entry->next->prev = entry;
		}
		else {
			entry = old_entry;
			if (entry->function_name) free(entry->function_name);
			if (entry->mask) free(entry->mask);
		}
	}
	else {
		for (old_entry = table->entries; old_entry && old_entry->next; old_entry = old_entry->next) {
			; /* empty loop */
		}
		entry = (bind_entry_t *) calloc(1, sizeof(*entry));
		if (old_entry) old_entry->next = entry;
		else table->entries = entry;
		entry->prev = old_entry;
	}

/*	if (flags) flag_from_str(&entry->user_flags, flags); */
	if (flags) {
		entry->user_flags.match = FR_GLOBAL | FR_CHAN;
		break_down_flags(flags, &entry->user_flags, NULL);
	}
        entry->cflags = cflags;
	if (mask) entry->mask = strdup(mask);
	if (function_name) entry->function_name = strdup(function_name);
	entry->callback = (HashFunc) callback;
	entry->client_data = client_data;
	entry->flags = bind_flags;

	return(0);
}

/* Execute a bind entry with the given argument list. */
static int bind_entry_exec(bind_table_t *table, bind_entry_t *entry, void **al)
{
	bind_entry_t *prev = NULL;
	int retval;

	ContextNote("bind", entry->mask);
	/* Give this entry a hit. */
	entry->nhits++;

	/* Does the callback want client data? */
	if (entry->flags & BIND_WANTS_CD) {
		*al = entry->client_data;
	}
	else al++;

	retval = entry->callback(al[0], al[1], al[2], al[3], al[4], al[5], al[6], al[7], al[8], al[9]);

	if (table->match_type & (MATCH_MASK | MATCH_PARTIAL | MATCH_NONE)) return(retval);

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

	return(retval);
}

int check_bind(bind_table_t *table, const char *match, struct flag_record *flags, ...)
{
	va_list args;
	int ret;

	va_start (args, flags);
	ret = bind_vcheck_hits (table, match, flags, NULL, args);
	va_end (args);

	return ret;
}

int check_bind_hits(bind_table_t *table, const char *match, struct flag_record *flags, int *hits, ...)
{
        va_list args;
        int ret;

        va_start (args, hits);
        ret = bind_vcheck_hits (table, match, flags, hits, args);
        va_end (args);

	return ret;
}

static int bind_vcheck_hits (bind_table_t *table, const char *match, struct flag_record *flags, int *hits, va_list ap)
{
	void *args[11] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	bind_entry_t *entry = NULL, *next = NULL, *winner = NULL;
	int cmp, retval, tie = 0;
        size_t matchlen = 0;

	check_bind_executing++;

	if (hits) (*hits) = 0;

	/* Default return value is 0 */
	retval = 0;

	if (!table) return retval;

	for (int i = 1; i <= table->nargs; i++) {
		args[i] = va_arg(ap, void *);
	}

	/* Check if we're searching for a partial match. */
	if (table->match_type & MATCH_PARTIAL) matchlen = strlen(match);

	for (entry = table->entries; entry; entry = next) {
		next = entry->next;
		if (entry->flags & BIND_DELETED) continue;

		/* Check flags. */
		if (table->match_type & MATCH_FLAGS) {
/*wtf?			if (!(entry->user_flags.builtin | entry->user_flags.udef)) cmp = 1;
			else if (!user_flags) cmp = 0;
			else
*/
			if (entry->flags & MATCH_FLAGS_AND) cmp = flagrec_eq(&entry->user_flags, flags);
			else cmp = flagrec_ok(&entry->user_flags, flags);
			if (!cmp) continue;
		}


		if (table->match_type & MATCH_MASK) {
			cmp = !wild_match_per(entry->mask, (char *) match);
		}
		else if (table->match_type & MATCH_NONE) {
			cmp = 0;
		}
		else if (table->match_type & MATCH_PARTIAL) {
			cmp = 1;
			if (!strncasecmp(match, entry->mask, matchlen)) {
				winner = entry;
				/* Is it an exact match? */
				if (!entry->mask[matchlen]) {
					tie = 1;
					break;
				}
				else tie++;
			}
		}
		else {
			if (table->match_type & MATCH_CASE) cmp = strcmp(entry->mask, match);
			/* MATCH_EXACT */
			else cmp = strcasecmp(entry->mask, match);
		}
		if (cmp) continue; /* Doesn't match. */

		if (hits) (*hits)++;

		retval = bind_entry_exec(table, entry, args);
		if ((table->flags & BIND_BREAKABLE) && (retval & BIND_RET_BREAK)) break;
	}
	/* If it's a partial match table, see if we have 1 winner. */
	if (winner && tie == 1) {
		if (hits) (*hits)++;
		retval = bind_entry_exec(table, winner, args);
	} else if (hits && tie) (*hits) = tie;

	check_bind_executing--;
	return(retval);
}

void add_builtins(const char *table_name, cmd_t *cmds)
{
	char name[50] = "";
	bind_table_t *table = bind_table_lookup_or_fake(table_name);

	for (; cmds->name; cmds++) {
          if (have_cmd(cmds->name, cmds->type)) {
                /* add BT_dcc cmds to cmdlist[] :: add to the help system.. */
                if (!strcmp(table->name, "dcc") && (findhelp(cmds->name) != NULL)) {
                  cmdlist[cmdi].name = cmds->name;
				  cmdlist[cmdi].type = cmds->type;
                  cmdlist[cmdi].flags.match = FR_GLOBAL | FR_CHAN;
                  break_down_flags(cmds->flags, &(cmdlist[cmdi].flags), NULL);
                  cmdi++;
                }
		simple_snprintf(name, sizeof name, "*%s:%s", table->name, cmds->funcname ? cmds->funcname : cmds->name);
		bind_entry_add(table, cmds->flags, cmds->type, cmds->name, name, 0, cmds->func, NULL);
          }
	}
}

void rem_builtins(const char *table_name, cmd_t *cmds)
{
	bind_table_t *table = bind_table_lookup(table_name);

	if (!table)
          return;

	char name[50] = "";

	for (; cmds->name; cmds++) {
		simple_snprintf(name, sizeof(name), "*%s:%s", table->name, cmds->funcname ? cmds->funcname : cmds->name);
		bind_entry_del(table, -1, cmds->name, name, NULL);
	}
}
/* vim: set sts=0 sw=8 ts=8 noet: */
