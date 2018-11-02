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

static inline bool
list_append(struct list_type **h, struct list_type *i)
{
  for (; *h; h = &((*h)->next))
    ;
  *h = i;
  return 1;
}

static inline bool
list_delete(struct list_type **h, struct list_type *i)
{
  for (; *h; h = &((*h)->next))
    if (*h == i) {
      *h = i->next;
      return 1;
    }
  return 0;
}

static inline bool __attribute__((pure))
list_contains(const struct list_type *h, const struct list_type *i)
{
  for (; h; h = h->next)
    if (h == i) {
      return 1;
    }
  return 0;
}

namespace bd {
  class Stream;
}

/* New userfile format stuff
 */
struct userrec;
struct user_entry;
struct user_entry_type {
  struct user_entry_type *next;
  bool (*got_share) (struct userrec *, struct user_entry *, char *, int);
  bool (*unpack) (struct userrec *, struct user_entry *);
  void (*write_userfile) (bd::Stream&, const struct userrec *, const struct user_entry *, int);
  bool (*kill) (struct user_entry *);
  void *(*get) (struct userrec *, struct user_entry *);
  bool (*set) (struct userrec *, struct user_entry *, void *);
  void (*display) (int idx, struct user_entry *, struct userrec *);
  const char *name;
};


extern struct user_entry_type USERENTRY_COMMENT, USERENTRY_LASTON,
 USERENTRY_INFO, USERENTRY_BOTADDR, USERENTRY_HOSTS,
 USERENTRY_PASS, USERENTRY_STATS, USERENTRY_ADDED, USERENTRY_MODIFIED,
 USERENTRY_SET, USERENTRY_SECPASS, USERENTRY_USERNAME, USERENTRY_NODENAME,
 USERENTRY_OS, USERENTRY_PASS1, USERENTRY_ARCH, USERENTRY_OSVER,
 USERENTRY_FFLAGS;

struct laston_info {
  time_t laston;
  char *lastonplace;
};

struct bot_addr {
  char *address;
  char *uplink;
  unsigned short hublevel;
  in_port_t telnet_port;
  in_port_t relay_port;
};

struct xtra_key {
  struct xtra_key *next;
  char *key;
  char *data;
};

struct user_entry {
  struct user_entry *next;
  struct user_entry_type *type;
  union {
    char *string;
    void *extra;
    struct xtra_key *xk;
    struct list_type *list;
    unsigned long ulong;
  } u;
  char *name;
};

struct filesys_stats {
  int uploads;
  int upload_ks;
  int dnloads;
  int dnload_ks;
};

bool add_entry_type(struct user_entry_type *);
struct user_entry_type *find_entry_type(const char *) __attribute__((pure));
struct user_entry *find_user_entry(struct user_entry_type *, struct userrec *);
void *get_user(struct user_entry_type *, struct userrec *);
bool user_has_host(const char *, struct userrec *, char *);
bool user_has_matching_host(const char *handle, struct userrec *u, char *host);
bool set_user(struct user_entry_type *, struct userrec *, void *);

#define is_bot(u)	((u) && (u)->bot)

/* Fake users used to store ignores and bans
 */
#define IGNORE_NAME "*ignore"
#define BAN_NAME    "*ban"
#define EXEMPT_NAME "*exempt"
#define INVITE_NAME "*Invite"
#define CHANS_NAME  "*channels"
#define SET_NAME "*Set"

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
  int fflags;
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

/* All the default userentry stuff, for code re-use
 */
bool def_unpack(struct userrec *u, struct user_entry *e);
bool def_kill(struct user_entry *e);
bool def_write_userfile(FILE *f, struct userrec *u, struct user_entry *e);
void *def_get(struct userrec *u, struct user_entry *e) __attribute__((pure));
bool def_set(struct userrec *u, struct user_entry *e, void *buf);
bool def_gotshare(struct userrec *u, struct user_entry *e, char *data, int idx);
void def_display(int idx, struct user_entry *e, struct userrec *u);


void backup_userfile();
void addignore(char *, char *, const char *, time_t);
char *delignore(char *);
void tell_ignores(int, char *);
bool match_ignore(const char *) __attribute__((pure));
void check_expired_ignores();
void autolink_cycle();
void tell_file_stats(int, char *);
void tell_user_ident(int, char *);
void tell_users_match(int, char *, int, int, char *, int);
int readuserfile(const char *, struct userrec **);
int stream_readuserfile(bd::Stream&, struct userrec **);
void check_pmode();
void link_pref_val(struct userrec *u, char *lval);
void check_stale_dcc_users();

extern char			userfile[], autolink_failed[];
extern interval_t			ignore_time;
extern bool			dont_restructure;

#endif				/* _EGG_USERS_H */
