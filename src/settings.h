#include "main.h"
#include "eggdrop.h"
#include <sys/utsname.h>

#define SETTINGS_SIZE 3072
#define NAME_HUBLIST ('h' << 8 | 'l')
#define NAME_ME ('m' << 8 | 'e')
#define NAME_IP ('i' << 8 | 'p')
#define NAME_USERLIST ('u' << 8 | 'l')
#define NAME_HOST ('h' << 8 | 'o')
#define NAME_MKEY ('m' << 8 | 'k')
#define NAME_PACKNAME ('p' <<8 | 'n')

typedef struct settings_entry {
  int name;
  int size;
  byte data[16];
} tag_settings_entry;

typedef struct settings_data {
  char prefix[16];
  char data[SETTINGS_SIZE - 32];
  char suffix[16];
  char hostdata[256];
} tag_settings_data;

/*
int get_setting(int name, char * value, int vlen);
*/
