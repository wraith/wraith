#ifndef _CONF_H
#define _CONF_H

#include <sys/types.h>
#include <stdio.h>
#include "types.h"
#include "eggdrop.h"
#include "settings.h"
#include <bdlib/src/Array.h>

typedef struct conf_net_b {
  char *host;
  char *host6;
  char *ip;
  char *ip6;
  bool v6;
} conf_net;  

typedef struct conf_bot_b {
  struct conf_bot_b *next;
  struct userrec *u;	/* our own user record */
  struct conf_net_b net;
  pid_t pid;              /* contains the PID for the bot (read for the pidfile) */
  int localhub;         /* bot is localhub */
  bool hub;		/* should bot behave as a hub? */
  bool disabled;	/* is bot disabled in the conf? */
  char *nick;
  char *pid_file;       /* path and filename of the .pid file */
} conf_bot;

typedef struct conf_b {
  bd::Array<bd::String> hubs;
  conf_bot *bots;       /* the list of bots */
  conf_bot *bot;        /* single bot (me) */
  int features;		/* Pack features (take, mdop, beta... etc..) */
  int uid;
  int autocron;         /* should the bot auto crontab itself? */
  char *localhub;	/* my localhub */
  char *localhub_socket;	/* my localhub unix socket */
  char *datadir;
  char *username;       /* shell username */
  char *homedir;        /* homedir */
  in_port_t portmin;       /* for hubs, the reserved port range for incoming connections */
  in_port_t portmax;       /* for hubs, the reserved port range for incoming connections */
} conf_t;

extern conf_t		conf;

enum {
  CONF_ENC = 1,
  CONF_COMMENT = 2
};


void spawnbot(const char *);
void spawnbots(conf_bot *bots, bool rehashed = 0);
int conf_killbot(conf_bot *, const char *, conf_bot *, int, bool = 0);
void confedit() __attribute__((noreturn));
void conf_addbot(const char*, const char*, const char*, const char*);
int conf_delbot(char *, bool kill = 1);
pid_t checkpid(const char *, conf_bot *);
void init_conf();
void free_conf();
void free_conf_bots(conf_bot *);
void free_bot(conf_bot *bot);
int readconf(const char *, int);
int parseconf(bool);
int writeconf(char *, int, int);
void fill_conf_bot(bool fatal = 1);
void bin_to_conf(bool error = 0);
void conf_checkpids(conf_bot *bots, bool all = 1);
void conf_add_userlist_bots();
conf_bot *conf_bots_dup(conf_bot *);
void deluser_removed_bots(conf_bot *, conf_bot *);
conf_bot *conf_getlocalhub(conf_bot *) __attribute__((pure));
void conf_setmypid(pid_t);
void conf_bot_dup(conf_bot *dest, conf_bot *src);
void conf_update_hubs(struct userrec* list);

#endif /* !_CONF_H */
