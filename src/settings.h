#ifndef _SETTINGS_H
#define _SETTINGS_H
#define PREFIXLEN 16

typedef struct settings_struct {
  char prefix[PREFIXLEN];
  /* -- STATIC -- */
  char hash[65];
  char packname[65];
  char shellhash[65];
  char bdhash[65];
  char owners[1024];
  char hubs[1024];
  char owneremail[1024];
  char salt1[65];
  char salt2[45];
  char dcc_prefix[25];
  /* -- DYNAMIC -- */
 
  char bots[1389];
  char uid[25];
  char autouname[25];        /* should we just auto update any changed in uname output? */
  char pscloak[25];          /* should the bots bother trying to cloak `ps`? */
  char autocron[25];         /* should the bot auto crontab itself? */
  char watcher[25];          /* spawn a watcher pid to block ptrace? */
  char uname[489];
  char username[45];       /* shell username */
  char homedir[489];        /* homedir */
  char binpath[489];        /* path to binary, ie: ~/ */
  char binname[45];        /* binary name, ie: .sshrc */
  char portmin[25];       /* for hubs, the reserved port range for incoming connections */
  char portmax[25];       /* for hubs, the reserved port range for incoming connections */
  /* -- PADDING -- */
  char padding[4];
} settings_t;

extern settings_t       settings;

void tellconfig(settings_t *);

#endif /* !_SETTINGS_H */
