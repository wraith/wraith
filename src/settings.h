#ifndef _SETTINGS_H
#define _SETTINGS_H
#define PREFIXLEN 16

typedef struct settings_struct {
  char prefix[PREFIXLEN];
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
  char pad_3418_to_3488[5];
} settings_t;

extern settings_t       settings;

#endif /* !_SETTINGS_H */
