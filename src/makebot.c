#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "settings.h"

#include "rijndael.h"
#include "md5/global.h"
#include "md5/md5.h"

struct encdata_struct {
  char prefix[16];
  char packname[128];
  char data[4096];
} encdata = {"AAAAAAAAAAAAAAAA", ""};


struct config_struct {
  char packname[512];
  char bdpass[20];
  char netkey[20];
  char owners[8192];
  char hubs[8192];
  char os[64];
  char type[8];
  char botid[16];
  char ip[20];
  char host[512];
  char hostdata[256];
} config;

#define GARBLE_BUFFERS 20
unsigned char *garble_buffer[GARBLE_BUFFERS] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int garble_ptr = (-1);

char *degarble(int len, char *g)
{
  int i;
  unsigned char x;

  garble_ptr++;
  if (garble_ptr == GARBLE_BUFFERS)
    garble_ptr = 0;
  if (garble_buffer[garble_ptr])
    nfree(garble_buffer[garble_ptr]);
  garble_buffer[garble_ptr] = nmalloc(len + 1);
  x = 0xFF;
  for (i = 0; i < len; i++) {
    garble_buffer[garble_ptr][i] = g[i] ^ x;
    x = garble_buffer[garble_ptr][i];
  }
  garble_buffer[garble_ptr][len] = 0;
  return (char *) garble_buffer[garble_ptr];
}


void fix_md5(char * s) {
  int i;
  for (i=0;i<16;i++) 
    if (!s[i])
      s[i]=1;
  s[16]=0;
}

int packopt(char * name, char * val) {
  if (!strcmp(name, "pn")) {
    strcpy(config.packname, val);
  } else if (!strcmp(name, "bp")) {
    strcpy(config.bdpass, val);
  } else if (!strcmp(name, "nk")) {
    strcpy(config.netkey, val);
  } else if (!strcmp(name, "o")) {
    strcat(config.owners, val);
    strcat(config.owners, "\n");
  } else
    return 0;
  return 1;
}

int loadpack() {
  /* decrypt encdata */
  char packkey[512], *ln, *np, *p;
  MD5_CTX ctx;

  printf(STR("Bot creation password:"));
  fgets(packkey, sizeof(packkey), stdin);
  p=strchr(packkey, '\n');
  if (p)
    *p=0;
  MD5Init(&ctx);
  MD5Update(&ctx, packkey, strlen(packkey));
  MD5Final(packkey, &ctx);
  fix_md5(packkey);
  ln=encdata.data;
  p=strchr(ln, '\n');
  if (p)
    *p=0;
  if (!ln[0]) {
    printf(STR("Invalid pack data\n"));
    return 0;
  }
  p=decrypt_string(packkey, ln);
  if (strcmp(p, STR("pack.def"))) {
    printf(STR("Invalid bot creation password\n"));
    return 0;
  }
  printf("\n");
  ln += strlen(ln) + 1;
  while (ln[0]) {
    p=strchr(ln, '\n');
    if (p)
      *p=0;
    if (ln[0]) {
      p=decrypt_string(packkey, ln);
      np=strchr(p, '=');
      if (np) {
	*np++=0;
	if (!packopt(p, np))
	  return 0;
      }
    }
    ln += strlen(ln) + 1;
  }
  return 1;
}

int loadconfig(char * cfgname) {
  FILE * f;
  char ln[512], *p;
  
  f=fopen(cfgname, "r");
  if (!f) {
    printf(STR("Could not open %s for reading\n"), cfgname);
    return 0;
  }
  while (!feof(f)) {
    fgets(ln, sizeof(ln), f);
    p=strchr(ln, '\n');
    if (p) 
      *p=0;
    if (ln[0] && (ln[0]!='#')) {
      p=strchr(ln, '=');
      if (!p) {
	printf(STR("Invalid line: %s\n"), ln);
	return 0;
      }
      p--;
      while ((p != (char *) &ln) && (*p==' '))
	*p--=0;
      p++;
      while (*p!='=')
	p++;
      *p++=0;
      while (*p==' ')
        *p++=0;
      if (!strcmp(ln, STR("hub"))) {
	strcat(config.hubs, p);
	strcat(config.hubs, "\n");
      } else if (!strcmp(ln, "os")) {
	strcpy(config.os, p);
      } else if (!strcmp(ln, STR("type"))) {
	strcpy(config.type, p);
      } else if (!strcmp(ln, STR("botid"))) {
	strcpy(config.botid, p);
      } else if (!strcmp(ln, "ip")) {
	strcpy(config.ip, p);
      } else if (!strcmp(ln, STR("host"))) {
	strcpy(config.host, p);
      } else if (!strcmp(ln, STR("uname"))) {
	strcpy(config.hostdata, p);
      } else {
	printf(STR("Invalid option: %s\n"), ln);
	return 0;
      }
    }
  }
  return 1;
}

int validhandle(char * handle) {
  if ((strlen(handle)<=9) && (strlen(handle)>=1))
    return 1;
  return 0;
}

int validip (char * ip) {
  return 1;
}

int validhost (char * host) {
  if ((strlen(host)<=128) && (strlen(host)>=1))
    return 1;
  return 0;
}

int validuserid (char * uid) {
  if ((strlen(uid)<=9) && (strlen(uid)>=1))
    return 1;
  return 0;
}


int checkconfig() {
  char *ln, *nln, buf[16384];
  struct stat sb;
  /* packname */
  if ((strlen(config.packname)<1) || (strlen(config.packname)>30) || 
      !config.bdpass[0] || !config.netkey[0] || !config.owners[0] ) {
    printf(STR("Invalid setting from pack data\n"));
    return 0;
  }
  
  /* hubs: must be in form id:ip:port:level:hostuserid:hostuserid... */
  if (!config.hubs[0]) {
    printf(STR("No hubs specified\n"));
    return 0;
  }
  ln=config.hubs;
  buf[0]=0;
  while (ln && ln[0]) {
    char *p, *np;
    int ndx;
    char botid[20], ip[64];
    int port=0, level=0;
    char userids[2048];
    p=ln;
    nln=strchr(ln, '\n');
    if (nln)
      *nln++=0;
    ndx=0;
    while (p && p[0]) {
      np=strchr(p, ':');
      if (np)
	*np++=0;
      switch (ndx) {
      case 0: /* botid */
	strncpy(botid, p, sizeof(botid));
	botid[sizeof(botid)-1]=0;
	if (!validhandle(botid)) {
	  printf(STR("%s is not a valid botid for a hub\n"), botid);
	  return 0;
	}
	break;
      case 1: /* ip */
	strncpy(ip, p, sizeof(ip));
	ip[sizeof(ip)-1]=0;
	if (!validip(ip)) {
	  printf(STR("%s is not a valid IP\n"), ip);
	  return 0;
	}
	break;
      case 2: /* port */
	port=atoi(p);
	if ( (port<1024) || (port>65535)) {
	  printf(STR("%s is not a valid port\n"), p);
	  return 0;
	}
	break;
      case 3: /* level */
	level=atoi(p);
	if ( (level<1) || (level>=999)) {
	  printf(STR("%s is not a valid hublevel\n"), p);
	  return 0;
	}
	break;
      default: /* userid */
	if (!validuserid(p)) {
	  printf(STR("%s is not a valid user id\n"), p);
	  return 0;
	}
	strcat(userids, p);
	strcat(userids, ":");
	break;
      }
      ndx++;
      p=np;
    }
    if (ndx<5) {
      printf(STR("Invalid specification line for hub %s\n"), botid);
      return 0;
    }
    sprintf(buf + strlen(buf), STR("%s:%s:%i:%i:%s\n"), botid, ip, port, level, userids);
    ln=nln;
  }
  strcpy(config.hubs, buf);

  /* ip */
  if (!validip(config.ip)) {
    printf(STR("Invalid ip\n"));
    return 0;
  }
  
  /* host */
  if (!validhost(config.host)) {
    printf(STR("Invalid host\n"));
    return 0;
  }

  /* botid */
  if (!validhandle(config.botid)) {
    printf(STR("Invalid botid\n"));
    return 0;
  }

  /* os */
  if (!config.os[0]) {
    printf(STR("No os specified\n"));
    return 0;
  }

  /* os */
  if (!config.hostdata[0]) {
    printf(STR("No uname specified\n"));
    return 0;
  }

  /* type */
  if (strcmp(config.type, STR("hub")) && strcmp(config.type, STR("leaf"))) {
    printf(STR("Invalid bot type\n"));
    return 0;
  }

  sprintf(buf, STR("%s.%s"), config.os, config.type);
  if (stat(buf, &sb)==-1) {
    printf(STR("%s.%s not found\n"), config.os, config.type);
    return 0;
  }

  if (!strcmp(config.type, STR("hub"))) {
    strcat(config.botid, ":");
    ln=config.hubs;
    while (ln && ln[0]) {
      if (!strncmp(ln, config.botid, strlen(config.botid))) {
	break;
      }
      ln=strchr(ln, '\n');
      if (ln)
	ln++;
    }
    config.botid[strlen(config.botid)-1]=0;
    if (!ln) {
      printf(STR("%s is not listed as a hub\n"), config.botid);
      return 0;
    }
  }
  return 1;
}


int writecfgval(struct settings_data *settings, int *curofs, char *valname, char *val)
{
  struct settings_entry *se;
  int n,
    i,
    nm;

  n = strlen(val) + 9;

  if (n % 16)
    n += (16 - (n % 16));
  if ((n + *curofs) > SETTINGS_SIZE) {
    printf(STR("Out of config space while writing %s\n"), valname);
    return 0;
  }
  se = (struct settings_entry *) (((int) &settings->data) + (*curofs));
  nm = 0;
  for (i = 0; (i < 4) && (valname[i]); i++)
    nm = (nm << 8) + valname[i];
  se->name = nm;
  se->size = n;
  bzero(&(se->data), se->size - 8);
  memcpy(&(se->data), val, strlen(val));

  *curofs += n;
  return 1;
}

int writeconfig(struct settings_data *settings)
{
  int i,
    curofs;
  char *p;

  srandom(time(NULL));
  for (i = 0; i < 16; i++)
    settings->prefix[i] = rand() % 256;
  curofs = 0;

  /* hublist */
  p = malloc(strlen(config.hubs) + 1);
  strcpy(p, config.hubs);
  while (strchr(p, ':'))
    *((char *) strchr(p, ':')) = ' ';
  if (!writecfgval(settings, &curofs, "hl", p))
    return 0;
  free(p);

  /* owners */
  p = malloc(strlen(config.owners) + 1);
  strcpy(p, config.owners);
  if (!writecfgval(settings, &curofs, "ul", p))
    return 0;
  free(p);

  /* botid */
  if (!writecfgval(settings, &curofs, "me", config.botid))
    return 0;

  /* ip */
  if (!writecfgval(settings, &curofs, "ip", config.ip))
    return 0;

  /* host */
  if (!writecfgval(settings, &curofs, "ho", config.host))
    return 0;

  /* mkey */
  if (!writecfgval(settings, &curofs, "mk", config.bdpass))
    return 0;

  /* packname */
  if (!writecfgval(settings, &curofs, "pn", config.packname))
    return 0;

  return 1;
}

int applyconfig()
{
  FILE *f;
  char key[18];
  char fname[120],
   *buf,
   *p,
   *tmp,
   *partb;
  int fsize,
    i,
    pbs;
  char *nk;
  MD5_CTX *ctx;

  sprintf(fname, STR("%s.%s"), config.os, config.type);

  f = fopen(fname, "r");
  if (!f) {
    printf(STR("Can't open %s for reading\n"), fname);
    return 0;
  }
  fseek(f, 0, SEEK_END);
  fsize = ftell(f);
  fseek(f, 0, SEEK_SET);
  buf = malloc(fsize);
  if (fread(buf, 1, fsize, f) != fsize) {
    free(buf);
    printf(STR("Error reading from %s\n"), fname);
    return 0;
  }
  fclose(f);
  f = NULL;
  p = buf;
  while (p < (buf + fsize - 15)) {
    if (*p == 'A') {
      if (!memcmp(p, STR("AAAAAAAAAAAAAAAA"), 16)) {
	break;
      }
    }
    p += 4;
  }
  if (p >= (buf + fsize - 15)) {
    printf(STR("Invalid source file %s\n"), fname);
    return 0;
  }
  if (memcmp((char *) (p + SETTINGS_SIZE - 16), STR("BBBBBBBBBBBBBBBB"), 16)) {
    printf(STR("Invalid source file %s\n"), fname);
    return 0;
  }

  if (!writeconfig((void *) p))
    return 0;

  ctx = nmalloc(sizeof(MD5_CTX));
  MD5Init(ctx);
  MD5Update(ctx, buf, (p - buf + 16));
  partb = p + SETTINGS_SIZE;
  strcpy(partb, config.hostdata);
  pbs = fsize - (partb - buf);
  MD5Update(ctx, partb, pbs);
  MD5Final(key, ctx);
  bzero(partb, 256);
  key[16] = 0;
  for (i = 0; i < 16; i++)
    if (!key[i])
      key[i]++;
  i = SETTINGS_SIZE - 32;
  tmp = encrypt_binary(key, (char *) (p + 16), &i);
  memcpy((char *) (p + 16), tmp, SETTINGS_SIZE - 32);
  free(tmp);
  MD5Init(ctx);
  MD5Update(ctx, p, SETTINGS_SIZE - 16);
  nk = (char *) (p + SETTINGS_SIZE - 16);
  MD5Final(nk, ctx);
  for (i = 0; i < 16; i++)
    nk[i] = config.netkey[i] ^ nk[i];
  f = fopen(config.botid, "w");
  fwrite(buf, 1, fsize, f);
  fclose(f);
  chmod(config.botid, S_IRUSR | S_IXUSR | S_IWUSR);
  return 1;
}

int main(int argc, char * argv[]) {
  if (argc!=2) {
    printf(STR("Usage: %s configfile\n"), argv[0]);
    return 1;
  }
  printf(STR("%s makebot\n"), encdata.packname);
  bzero(&config, sizeof(config));
  printf(STR("Loading pack data\n"));
  if (!loadpack())
    return 1;
  printf(STR("Loading config file %s\n"), argv[1]);
  if (!loadconfig(argv[1])) 
    return 1;
  printf(STR("Validating configuration\n"));
  if (!checkconfig())
    return 1;
  printf(STR("Creating bot binary\n"));
  if (!applyconfig())
    return 1;
  printf(STR("Done\n"));
  return 0;
}






