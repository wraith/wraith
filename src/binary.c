/*
 * binary.c -- handles:
 *   misc update functions
 *   md5 hash verifying
 *
 */

#include "common.h"
#include "binary.h"
#include "settings.h"
#include "crypt.h"
#include "shell.h"
#include "misc.h"
#include "main.h"
#include "misc_file.h"

/*
typedef struct encdata_struct {
  char prefix[PREFIXLEN];
  char data[65];
} encdata_t;

static encdata_t encdata = {
  "AAAAAAAAAAAAAAAA",
  ""
};
*/

settings_t settings = {
  "AAAAAAAAAAAAAAA",
  /* -- STATIC -- */
  "", "", "", "", "", "", "", "", "", "",
  /* -- DYNAMIC -- */
  "", "", "", "", "", "", "", "", "", "", "", "", "",
  /* -- PADDING */
  ""
};

#define PACK_ENC 1
#define PACK_DEC 2
static void edpack(settings_t *, const char *, int);

int checked_bin_buf = 0;

static char *
bin_md5(const char *fname, int todo, MD5_CTX * ctx)
{
  static char hash[MD5_HASH_LENGTH + 1] = "";
  unsigned char md5out[MD5_HASH_LENGTH + 1] = "", buf[PREFIXLEN + 1] = "";
  FILE *f = NULL;
  size_t len = 0;

  checked_bin_buf++;
  if (!(f = fopen(fname, "rb")))
    werr(ERR_BINSTAT);

  while ((len = fread(buf, 1, sizeof buf - 1, f))) {
    if (!memcmp(buf, &settings.prefix, PREFIXLEN)) {
      break;
    }
    MD5_Update(ctx, buf, len);
  }

  fclose(f);
  MD5_Final(md5out, ctx);
  strncpyz(hash, btoh(md5out, MD5_DIGEST_LENGTH), sizeof(hash));
  OPENSSL_cleanse(&ctx, sizeof(ctx));

  if (todo == WRITE_MD5) {
    Tempfile *newbin = new Tempfile("bin");
    char *fname_bak = NULL;
    size_t size = 0, i = 0;

    size = strlen(fname) + 2;
    fname_bak = (char *) my_calloc(1, size);
    egg_snprintf(fname_bak, size, "%s~", fname);
    size = 0;

    if (!(f = fopen(fname, "rb")))
      werr(ERR_BINSTAT);

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    while ((len = fread(buf, 1, sizeof(buf) - 1, f))) {
      if (i) {                  /* to skip bytes for hash */
        i -= sizeof(buf) - 1;
        continue;
      }
      if (fwrite(buf, sizeof(buf) - 1, 1, newbin->f) != 1) {
        fclose(f);
        delete newbin;
        werr(ERR_BINSTAT);
      }
/*
      if (!memcmp(buf, &encdata.prefix, PREFIXLEN)) {
        // now we have 65 for data :D
        char *enc_hash = NULL;

        enc_hash = encrypt_string(SALT1, hash);
        fwrite(enc_hash, strlen(enc_hash), 1, fn);
        i = strlen(enc_hash);   // skip the next strlen(enc_hash) bytes
        free(enc_hash);
      }
*/
      if (!memcmp(buf, &settings.prefix, PREFIXLEN)) {
        strncpyz(settings.hash, hash, 65);
        edpack(&settings, hash, PACK_ENC);
        fwrite(&settings.hash, sizeof(settings_t) - PREFIXLEN, 1, newbin->f);
        i = sizeof(settings_t) - PREFIXLEN;
      }
    }

    fclose(f);

    if (movefile(fname, fname_bak))
      fatal("Crappy os :D", 0);

    if (movefile(newbin->file, fname))
      fatal("Crappy os :D", 0);

    fixmod(fname);
    unlink(fname_bak);
    delete newbin;
  }
  return hash;
}

static int
readcfg(const char *cfgfile)
{
  FILE *f = NULL;

  if ((f = fopen(cfgfile, "r")) == NULL) {
    printf("Error: Can't open '%s' for reading\n", cfgfile);
    exit(1);
  }

  char *buffer = NULL, *p = NULL;
  int skip = 0, line = 0;

  printf("Reading '%s' ", cfgfile);
  while ((!feof(f)) && ((buffer = step_thru_file(f)) != NULL)) {
    line++;
    if ((*buffer)) {
      if (strchr(buffer, '\n'))
        *(char *) strchr(buffer, '\n') = 0;
      if ((skipline(buffer, &skip)))
        continue;
      if (strchr(buffer, '<') || strchr(buffer, '>')) {
        printf(" Failed\n");
        printf("%s:%d: error: Look at your configuration file again...\n", cfgfile, line);
        exit(1);
      }
      p = strchr(buffer, ' ');
      while (p && (strchr(LISTSEPERATORS, p[0])))
        *p++ = 0;
      if (p) {
        if (!egg_strcasecmp(buffer, "packname")) {
          strncpyz(settings.packname, trim(p), sizeof settings.packname);
          printf(".");
        } else if (!egg_strcasecmp(buffer, "shellhash")) {
          strncpyz(settings.shellhash, trim(p), sizeof settings.shellhash);
          printf(".");
        } else if (!egg_strcasecmp(buffer, "bdhash")) {
          strncpyz(settings.bdhash, trim(p), sizeof settings.bdhash);
          printf(".");
        } else if (!egg_strcasecmp(buffer, "dccprefix")) {
          strncpyz(settings.dcc_prefix, trim(p), sizeof settings.dcc_prefix);
          printf(".");
        } else if (!egg_strcasecmp(buffer, "owner")) {
          strcat(settings.owners, trim(p));
          strcat(settings.owners, ",");
          printf(".");
        } else if (!egg_strcasecmp(buffer, "owneremail")) {
          strcat(settings.owneremail, trim(p));
          strcat(settings.owneremail, ",");
          printf(".");
        } else if (!egg_strcasecmp(buffer, "hub")) {
          strcat(settings.hubs, trim(p));
          strcat(settings.hubs, ",");
          printf(".");
        } else if (!egg_strcasecmp(buffer, "salt1")) {
          strcat(settings.salt1, trim(p));
          printf(".");
        } else if (!egg_strcasecmp(buffer, "salt2")) {
          strcat(settings.salt2, trim(p));
          printf(".");
        } else {
          printf("%s %s\n", buffer, p);
          printf(",");
        }
      }
    }
    buffer = NULL;
  }
  if (f)
    fclose(f);
  if (!settings.salt1[0] || !settings.salt2[0]) {
    /* Write salts back to the cfgfile */
    char salt1[SALT1LEN + 1] = "", salt2[SALT2LEN + 1] = "";

    printf("Creating Salts");
    if ((f = fopen(cfgfile, "a")) == NULL) {
      printf("Cannot open cfgfile for appending.. aborting\n");
      exit(1);
    }
    make_rand_str(salt1, SALT1LEN);
    make_rand_str(salt2, SALT2LEN);
    salt1[sizeof salt1] = salt2[sizeof salt2] = 0;
    fprintf(f, "SALT1 %s\n", salt1);
    fprintf(f, "SALT2 %s\n", salt2);
    fflush(f);
    fclose(f);
  }
  printf(" Success\n");
  return 1;
}

static void edpack(settings_t *incfg, const char *hash, int what)
{
  char *tmp = NULL;
  char *(*enc_dec_string)(const char *, char *);
  
  if (what == PACK_ENC)
    enc_dec_string = encrypt_string;
  else
    enc_dec_string = decrypt_string;

#define dofield(_field) 		do {		\
	tmp = enc_dec_string(hash, _field);		\
	egg_snprintf(_field, sizeof(_field), tmp);	\
	free(tmp);					\
} while (0)

  /* -- STATIC -- */
  dofield(incfg->hash);
  dofield(incfg->packname);
  dofield(incfg->shellhash);
  dofield(incfg->bdhash);
  dofield(incfg->dcc_prefix);
  dofield(incfg->owners);
  dofield(incfg->owneremail);
  dofield(incfg->hubs);
  /* -- DYNAMIC -- */
//printf("BOTS: %s\n", incfg->bots);
  dofield(incfg->bots);
//printf("EBOTS: %s\n", incfg->bots);
  dofield(incfg->uid);
  dofield(incfg->autouname);
  dofield(incfg->pscloak);
  dofield(incfg->autocron);
  dofield(incfg->watcher);
  dofield(incfg->uname);
  dofield(incfg->username);
  dofield(incfg->homedir);
  dofield(incfg->binpath);
  dofield(incfg->binname);
  dofield(incfg->portmin);
  dofield(incfg->portmax);
#undef dofield
}

/* 
static void
tellconfig(settings_t *incfg)
{
#define dofield(_field)		printf("%s: %s\n", #_field, _field);
  // -- STATIC --
  dofield(incfg->hash);
  dofield(incfg->packname);
  dofield(incfg->shellhash);
  dofield(incfg->bdhash);
  dofield(incfg->dcc_prefix);
  dofield(incfg->owners);
  dofield(incfg->owneremail);
  dofield(incfg->hubs);
  // -- DYNAMIC --
  dofield(incfg->bots);
  dofield(incfg->uid);
  dofield(incfg->autouname);
  dofield(incfg->pscloak);
  dofield(incfg->autocron);
  dofield(incfg->watcher);
  dofield(incfg->uname);
  dofield(incfg->username);
  dofield(incfg->homedir);
  dofield(incfg->binpath);
  dofield(incfg->binname);
  dofield(incfg->portmin);
  dofield(incfg->portmax);
#undef dofield
}
*/

void
check_sum(const char *fname, const char *cfgfile)
{
  MD5_CTX ctx;

  MD5_Init(&ctx);
 
  if (!settings.hash[0]) {

    if (!cfgfile)
      fatal("Binary not initialized.", 0);

    readcfg(cfgfile);

// tellconfig(&settings); 
    if (bin_md5(fname, WRITE_MD5, &ctx))
      printf("* Wrote settings to binary.\n"); 
    exit(0);
  } else {
    char *hash = bin_md5(fname, GET_MD5, &ctx);

// tellconfig(&settings); 
    edpack(&settings, hash, PACK_DEC);
// tellconfig(&settings); 

    if (strcmp(settings.hash, hash)) {
      unlink(fname);
      fatal("!! Invalid binary", 0);
    }
  }
}

void write_settings(const char *fname, int die)
{
  MD5_CTX ctx;
  char *hash = NULL;

  MD5_Init(&ctx);
  if ((hash = bin_md5(fname, WRITE_MD5, &ctx))) {
    printf("* Wrote settings to %s.\n", fname);
    edpack(&settings, hash, PACK_DEC);
  }

  if (die)
    exit(0);
}

static void 
clear_settings(void)
{
  memset(&settings.bots, 0, sizeof(settings_t) - 3467);
}

void conf_to_bin(conf_t *in)
{
  conf_bot *bot = NULL;
  char *newbin = NULL;

  clear_settings();
  sdprintf("converting conf to bin\n");
  sprintf(settings.uid, "%d", in->uid);
  sprintf(settings.watcher, "%d", in->watcher);
  sprintf(settings.autocron, "%d", in->autocron);
  sprintf(settings.autouname, "%d", in->autouname);
  sprintf(settings.portmin, "%d", in->portmin);
  sprintf(settings.portmax, "%d", in->portmax);
  sprintf(settings.pscloak, "%d", in->pscloak);

  strncpyz(settings.binname, in->binname, 16);
  strncpyz(settings.username, in->username, 16);

  strncpyz(settings.uname, in->uname, 350);
  strncpyz(settings.homedir, in->homedir, 350);
  strncpyz(settings.binpath, in->binpath, 350);
  for (bot = in->bots; bot && bot->nick; bot = bot->next) {
    sprintf(settings.bots, "%s%s %s %s%s %s,", settings.bots && settings.bots[0] ? settings.bots : "",
                           bot->nick,
                           bot->ip ? bot->ip : ".", 
                           bot->host6 ? "+" : "", 
                           bot->host ? bot->host : (bot->host6 ? bot->host6 : "."),
                           bot->ip6 ? bot->ip6 : "");
    }

  newbin = move_bin(in->binpath, in->binname, 0);
  /* tellconfig(&settings); */
  write_settings(newbin, 1);
}
