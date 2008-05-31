#ifndef _SETTINGS_H
#define _SETTINGS_H
#define PREFIXLEN 16

#define SETTINGS_VER 1

typedef struct settings_struct {
  char prefix[PREFIXLEN];
  /* -- STATIC -- */
//  char hash[33];
  char hash[49];
  char packname[65];
  char shellhash[65];
  char owners[513];
  char hubs[513];
  char owneremail[385];
  char salt1[33];
  char salt2[17];
  char dcc_prefix[17];
  char features[17];
  /* -- DYNAMIC -- */
  char bots[1025];
  char uid[17];
  char autouname[17];        /* should we just auto update any changed in uname output? */
  char pscloak[17];          /* should the bots bother trying to cloak `ps`? */
  char autocron[17];         /* should the bot auto crontab itself? */
  char watcher[17];          /* spawn a watcher pid to block ptrace? */
  char uname[113];
  char username[49];       /* shell username */
  char datadir[1025];
  char homedir[1025];        /* homedir */
  char binpath[1025];        /* path to binary, ie: ~/ */
  char binname[113];        /* binary name, ie: .sshrc */
  char portmin[17];       /* for hubs, the reserved port range for incoming connections */
  char portmax[17];       /* for hubs, the reserved port range for incoming connections */
  /* -- PADDING -- */
  char padding[8];
} settings_t;

#define SIZE_PACK sizeof(settings.hash) + sizeof(settings.packname) + sizeof(settings.shellhash) + \
sizeof(settings.owners) + sizeof(settings.hubs) + sizeof(settings.owneremail) + \
sizeof(settings.salt1) + sizeof(settings.salt2) + sizeof(settings.dcc_prefix) + sizeof(settings.features)

#define SIZE_CONF sizeof(settings.bots) + sizeof(settings.uid) + sizeof(settings.autouname) + \
sizeof(settings.pscloak) + sizeof(settings.autocron) + sizeof(settings.watcher) + sizeof(settings.uname) + \
sizeof(settings.username) + sizeof(settings.homedir) + sizeof(settings.binpath) + sizeof(settings.binname) + \
sizeof(settings.portmin) + sizeof(settings.portmin) + sizeof(settings.datadir)

#define SIZE_PAD sizeof(settings.padding)

#define SIZE_SETTINGS sizeof(settings_t)

extern settings_t       settings;

#endif /* !_SETTINGS_H */
