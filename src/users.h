/*
 * users.h
 *   structures and definitions used by users.c and userrec.c
 *
 */

#ifndef _EGG_USERS_H
#define _EGG_USERS_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* List functions :) , next *must* be the 1st item in the struct */
struct list_type {
  struct list_type *next;
  char *extra;
};

#define list_insert(a,b) {						\
    	(b)->next = *(a);						\
	*(a) = (b);							\
}
bool list_append(struct list_type **, struct list_type *);
bool list_delete(struct list_type **, struct list_type *);
bool list_contains(struct list_type *, struct list_type *);


/* New userfile format stuff
 */
struct userrec;
struct user_entry;
struct user_entry_type {
  struct user_entry_type *next;
  bool (*got_share) (struct userrec *, struct user_entry *, char *, int);
  bool (*unpack) (struct userrec *, struct user_entry *);
#ifdef HUB
  bool (*write_userfile) (FILE *, struct userrec *, struct user_entry *);
#endif /* HUB */
  bool (*kill) (struct user_entry *);
  void *(*get) (struct userrec *, struct user_entry *);
  bool (*set) (struct userrec *, struct user_entry *, void *);
  void (*display) (int idx, struct user_entry *, struct userrec *);
  const char *name;
};


extern struct user_entry_type USERENTRY_COMMENT, USERENTRY_LASTON,
 USERENTRY_INFO, USERENTRY_BOTADDR, USERENTRY_HOSTS,
 USERENTRY_PASS, USERENTRY_STATS, USERENTRY_ADDED, USERENTRY_MODIFIED,
  USERENTRY_CONFIG, USERENTRY_SECPASS, USERENTRY_USERNAME, USERENTRY_NODENAME, USERENTRY_OS;

struct laston_info {
  time_t laston;
  char *lastonplace;
};

struct bot_addr {
  unsigned int roleid;
  char *address;
  char *uplink;
  unsigned short hublevel;
  port_t telnet_port;
  port_t relay_port;
};

struct user_entry {
  struct user_entry *next;
  struct user_entry_type *type;
  union {
    char *string;
    void *extra;
    struct list_type *list;
    unsigned long ulong;
  } u;
  char *name;
};

struct xtra_key {
  struct xtra_key *next;
  char *key;
  char *data;
};

struct filesys_stats {
  int uploads;
  int upload_ks;
  int dnloads;
  int dnload_ks;
};

bool add_entry_type(struct user_entry_type *);
struct user_entry_type *find_entry_type(char *);
struct user_entry *find_user_entry(struct user_entry_type *, struct userrec *);
void *get_user(struct user_entry_type *, struct userrec *);
bool set_user(struct user_entry_type *, struct userrec *, void *);

#define is_bot(u)	((u) && (u)->bot)

/* Fake users used to store ignores and bans
 */
#define IGNORE_NAME "*ignore"
#define BAN_NAME    "*ban"
#define EXEMPT_NAME "*exempt"
#define INVITE_NAME "*Invite"
#define CHANS_NAME  "*channels"
#define CONFIG_NAME "*Config"

/* Channel-specific info
 */
struct chanuserrec {
  struct chanuserrec *next;
  flag_t flags;
  time_t laston;
  char *info;
  char channel[81];
};

/* New-style userlist
 */
struct userrec {
  struct user_entry *entries;
  struct chanuserrec *chanrec;
  struct userrec *next;
  flag_t flags;
  char handle[HANDLEN + 1];
  char bot;
};

struct igrec {
  struct igrec *next;
  time_t expire;
  time_t added;
  int flags;
  char *igmask;
  char *user;
  char *msg;
};
extern struct igrec *global_ign;

#define IGREC_PERM   2

/*
 * Note: Flags are in eggdrop.h
 */

struct userrec *get_user_by_handle(struct userrec *, char *);
struct userrec *get_user_by_host(char *);
struct userrec *check_chanlist(const char *);
struct userrec *check_chanlist_hand(const char *);

/* All the default userentry stuff, for code re-use
 */
bool def_unpack(struct userrec *u, struct user_entry *e);
bool def_kill(struct user_entry *e);
bool def_write_userfile(FILE *f, struct userrec *u, struct user_entry *e);
void *def_get(struct userrec *u, struct user_entry *e);
bool def_set(struct userrec *u, struct user_entry *e, void *buf);
bool def_gotshare(struct userrec *u, struct user_entry *e, char *data, int idx);
void def_display(int idx, struct user_entry *e, struct userrec *u);


#ifdef HUB
void backup_userfile();
#endif /* HUB */
void addignore(char *, char *, const char *, time_t);
int delignore(char *);
void tell_ignores(int, char *);
bool match_ignore(char *);
void check_expired_ignores();
void autolink_cycle(char *);
void tell_file_stats(int, char *);
void tell_user_ident(int, char *);
void tell_users_match(int, char *, int, int, char *, int);
int readuserfile(const char *, struct userrec **);
void check_pmode();
void link_pref_val(struct userrec *u, char *lval);

extern char			natip[], userfile[];
extern time_t			ignore_time;

#endif				/* _EGG_USERS_H */
