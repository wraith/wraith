/*
 * tclhash.h
 *
 */

#ifndef _EGG_TCLHASH_H
#define _EGG_TCLHASH_H

#include "cmds.h"

#define TC_DELETED	0x0001	/* This command/trigger was deleted.	*/

/* Match type flags for bind tables. */
#define MATCH_PARTIAL       1
#define MATCH_EXACT         2
#define MATCH_MASK          4
#define MATCH_CASE          8
#define MATCH_NONE          16
#define MATCH_FLAGS_AND	    32
#define MATCH_FLAGS_OR	    64
#define MATCH_FLAGS	    96


/* Flags for binds. */
/* Does the callback want their client_data inserted as the first argument? */
#define BIND_WANTS_CD 1
#define BIND_BREAKABLE	2
#define BIND_STACKABLE	4
#define BIND_DELETED	8
#define BIND_FAKE	16
/*** Note: bind entries can specify these two flags, defined above.
#define MATCH_FLAGS_AND	32
#define MATCH_FLAGS_OR	64
***/

/* Flags for return values from bind callbacks */
#define BIND_RET_LOG 1
#define BIND_RET_BREAK 2

/* This holds the information of a bind entry. */
typedef struct bind_entry_b {
	struct bind_entry_b *next, *prev;
        struct flag_record user_flags;
	char *mask;
        char *function_name;
        Function callback;
        void *client_data;
	int nhits;
        int flags;
	int id;
} bind_entry_t;

/* This is the highest-level structure. It's like the "msg" table
   or the "pubm" table. */
typedef struct bind_table_b {
        struct bind_table_b *next;
	bind_entry_t *entries;
        char *name;
        char *syntax;
        int nargs;
        int match_type;
        int flags;
} bind_table_t;


void kill_binds(void);
void binds_init();

int check_bind(bind_table_t *table, const char *match, struct flag_record *_flags, ...);
bind_table_t *bind_table_add(const char *name, int nargs, const char *syntax, int match_type, int flags);
void bind_table_del(bind_table_t *table);
bind_table_t *bind_table_lookup(const char *name);
bind_table_t *bind_table_lookup_or_fake(const char *name);
int bind_entry_add(bind_table_t *table, const char *flags, const char *mask, const char *function_name, int bind_flags, Function callback, void *client_data);
int bind_entry_del(bind_table_t *table, int id, const char *mask, const char *function_name, void *cdata);
int bind_entry_modify(bind_table_t *table, int id, const char *mask, const char *function_name, const char *newflags, const char *newmask);
int bind_entry_overwrite(bind_table_t *table, int id, const char *mask, const char *function_name, Function callback, void *client_data);
void add_builtins(const char *table_name, cmd_t *cmds);
void rem_builtins(const char *table_name, cmd_t *cmds);

#endif				/* _EGG_TCLHASH_H */
