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

#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

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
bin_checksum(const char *fname, int todo)
{
  MD5_CTX ctx;
  static char hash[MD5_HASH_LENGTH + 1] = "";
  unsigned char md5out[MD5_HASH_LENGTH + 1] = "", buf[PREFIXLEN + 1] = "";
  FILE *f = NULL;
  size_t len = 0;

  MD5_Init(&ctx);

  checked_bin_buf++;
 
  hash[0] = 0;

  if (todo == GET_CHECKSUM) {
    if (!(f = fopen(fname, "rb")))
      werr(ERR_BINSTAT);

    while ((len = fread(buf, 1, sizeof buf - 1, f))) {
      if (!memcmp(buf, &settings.prefix, PREFIXLEN))
        break;
      MD5_Update(&ctx, buf, len);
    }

    fclose(f);
    MD5_Final(md5out, &ctx);
    strlcpy(hash, btoh(md5out, MD5_DIGEST_LENGTH), sizeof(hash));
    OPENSSL_cleanse(&ctx, sizeof(ctx));
  }

  if (todo == GET_CONF) {
    char *oldhash = strdup(settings.hash);

    if (!(f = fopen(fname, "rb")))
      werr(ERR_BINSTAT);

    while ((len = fread(buf, 1, sizeof buf - 1, f)))
      if (!memcmp(buf, &settings.prefix, PREFIXLEN))
        break;

    char *tmpbuf = (char *) my_calloc(1, SIZE_PACK);
 
    if ((len = fread(tmpbuf, 1, SIZE_PACK, f))) {
      edpack(&settings, oldhash, PACK_ENC);
      len = fread(&settings.bots, 1, SIZE_CONF, f);
      edpack(&settings, oldhash, PACK_DEC);
    } else
      return NULL;

    free(oldhash);
    fclose(f);
    return settings.hash;
  }

  if (todo & WRITE_CHECKSUM) {
    Tempfile *newbin = new Tempfile("bin");
    char *fname_bak = NULL;
    size_t size = 0, newpos = 0;

    size = strlen(fname) + 2;
    fname_bak = (char *) my_calloc(1, size);
    egg_snprintf(fname_bak, size, "%s~", fname);
    size = 0;

    if (!(f = fopen(fname, "rb")))
      goto fatal;

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    newpos = 0;

    while ((len = fread(buf, 1, sizeof(buf) - 1, f))) {
      if (fwrite(buf, 1, len, newbin->f) != len)
        goto fatal;

      newpos += len;

      if (!memcmp(buf, &settings.prefix, PREFIXLEN)) {		/* found the settings struct! */
        MD5_Final(md5out, &ctx);
        strlcpy(hash, btoh(md5out, MD5_DIGEST_LENGTH), sizeof(hash));
        OPENSSL_cleanse(&ctx, sizeof(ctx));

        strlcpy(settings.hash, hash, 65);
        edpack(&settings, hash, PACK_ENC);		/* encrypt the entire struct with the hash (including hash) */

        if (todo & WRITE_PACK) {
          fwrite(&settings.hash, SIZE_PACK, 1, newbin->f);
          sdprintf("writing pack: %d\n", SIZE_PACK);
        } else {
          char *tmpbuf = (char *) my_calloc(1, SIZE_PACK);

          if ((len = fread(tmpbuf, 1, SIZE_PACK, f))) {
            if (fwrite(tmpbuf, 1, len, newbin->f) != len) {
              free(tmpbuf);
              goto fatal;
            }
          }
          free(tmpbuf);
        }
        newpos += SIZE_PACK;

        if (todo & WRITE_CONF) {
          fwrite(&settings.bots, SIZE_CONF, 1, newbin->f);
          sdprintf("writing conf: %d\n", SIZE_CONF);
        } else {
          char *tmpbuf = (char *) my_calloc(1, SIZE_CONF);

          if ((len = fread(tmpbuf, 1, SIZE_CONF, f))) {
            if (fwrite(tmpbuf, 1, len, newbin->f) != len) {
              free(tmpbuf);
              goto fatal;
            }
          }
          free(tmpbuf);
        }
        newpos += SIZE_CONF;

        fseek(newbin->f, newpos + SIZE_PAD, SEEK_SET);
        newpos += SIZE_PAD;

        /* skip reading over the stuff we already wrote */
        fseek(f, newpos, SEEK_SET);
      } else if (!hash[0])		/* hash as long as we haven't reached the prefix */
        MD5_Update(&ctx, buf, len);
    }

    fclose(f);

    if (size != newpos) {
      delete newbin;
      fatal("Binary corrupted", 0);
    }

    if (movefile(fname, fname_bak)) {
      printf("Failed to move file (%s -> %s): %s\n", fname, fname_bak, strerror(errno));
      delete newbin;
      fatal("", 0);
    }

    if (movefile(newbin->file, fname)) {
      printf("Failed to move file (%s -> %s): %s\n", newbin->file, fname, strerror(errno));
      delete newbin;
      fatal("", 0);
    }

    fixmod(fname);
    unlink(fname_bak);
    delete newbin;
    
    return hash;
  fatal:
    if (f)
      fclose(f);
    delete newbin;
    werr(ERR_BINSTAT);
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
          strlcpy(settings.packname, trim(p), sizeof settings.packname);
          printf(".");
        } else if (!egg_strcasecmp(buffer, "shellhash")) {
          strlcpy(settings.shellhash, trim(p), sizeof settings.shellhash);
          printf(".");
        } else if (!egg_strcasecmp(buffer, "bdhash")) {
          strlcpy(settings.bdhash, trim(p), sizeof settings.bdhash);
          printf(".");
        } else if (!egg_strcasecmp(buffer, "dccprefix")) {
          strlcpy(settings.dcc_prefix, trim(p), sizeof settings.dcc_prefix);
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
	if (_field && _field[0]) {			\
		tmp = enc_dec_string(hash, _field);		\
		egg_snprintf(_field, sizeof(_field), tmp);	\
		free(tmp);					\
	}						\
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
   if (!settings.hash[0]) {

    if (!cfgfile)
      fatal("Binary not initialized.", 0);

    readcfg(cfgfile);

// tellconfig(&settings); 
    if (bin_checksum(fname, WRITE_CHECKSUM|WRITE_CONF|WRITE_PACK))
      printf("* Wrote settings to binary.\n"); 
    exit(0);
  } else {
    char *hash = bin_checksum(fname, GET_CHECKSUM);

// tellconfig(&settings); 
    edpack(&settings, hash, PACK_DEC);
// tellconfig(&settings); 

    if (strcmp(settings.hash, hash)) {
      unlink(fname);
      fatal("!! Invalid binary", 0);
    }
  }
}

static bool check_bin_initialized(const char *fname)
{
  int i = 0;
  size_t len = strlen(fname) + 3 + 1;
  char *path = (char *) my_calloc(1, len);

  egg_snprintf(path, len, "%s -p", fname);

  i = system(path);
  free(path);
  if (i != -1 && WEXITSTATUS(i) == 4)
    return 1;

  return 0;
}

void write_settings(const char *fname, int die, bool conf)
{
  char *hash = NULL;
  int bits = WRITE_CHECKSUM;
  /* see if the binary is already initialized or not */
  bool initialized = check_bin_initialized(fname);

  /* only write pack data if the binary is uninitialized
   * otherwise, assume it has similar/correct/updated pack data
   */
  if (!initialized)
    bits |= WRITE_PACK;
  if (conf)
    bits |= WRITE_CONF;

  /* only bother writing anything if we have pack or conf, checksum is worthless to write out */
  if (bits & (WRITE_PACK|WRITE_CONF)) {
    if ((hash = bin_checksum(fname, bits))) {
      printf("* Wrote %ssettings to: %s.\n", ((bits & WRITE_PACK) && !(bits & WRITE_CONF)) ? "pack " :
                                             ((bits & WRITE_CONF) && !(bits & WRITE_PACK)) ? "conf " :
                                             ((bits & WRITE_PACK) && (bits & WRITE_CONF))  ? "pack/conf "  :
                                             "",
                                             fname);
      if (die == -1)			/* only bother decrypting if we aren't about to exit */
        edpack(&settings, hash, PACK_DEC);
    }
  }

  if (die >= 0)
    exit(die);
}

static void 
clear_settings(void)
{
  memset(&settings.bots, 0, sizeof(settings_t) - 3467);
}

void conf_to_bin(conf_t *in, bool move, int die)
{
  conf_bot *bot = NULL;
  char *newbin = NULL;

  clear_settings();
  sdprintf("converting conf to bin\n");
  simple_sprintf(settings.uid, "%d", in->uid);
  simple_sprintf(settings.watcher, "%d", in->watcher);
  simple_sprintf(settings.autocron, "%d", in->autocron);
  simple_sprintf(settings.autouname, "%d", in->autouname);
  simple_sprintf(settings.portmin, "%d", in->portmin);
  simple_sprintf(settings.portmax, "%d", in->portmax);
  simple_sprintf(settings.pscloak, "%d", in->pscloak);

  strlcpy(settings.binname, in->binname, 16);
  strlcpy(settings.username, in->username, 16);

  strlcpy(settings.uname, in->uname, 350);
  strlcpy(settings.homedir, in->homedir, 350);
  strlcpy(settings.binpath, in->binpath, 350);
  for (bot = in->bots; bot && bot->nick; bot = bot->next) {
    simple_sprintf(settings.bots, "%s%s %s %s%s %s,", settings.bots && settings.bots[0] ? settings.bots : "",
                           bot->nick,
                           bot->net.ip ? bot->net.ip : ".", 
                           bot->net.host6 ? "+" : "", 
                           bot->net.host ? bot->net.host : (bot->net.host6 ? bot->net.host6 : "."),
                           bot->net.ip6 ? bot->net.ip6 : "");
    }

  if (move)
    newbin = move_bin(in->binpath, in->binname, 0);
  else
    newbin = binname;
  /* tellconfig(&settings); */
  write_settings(newbin, -1, 1);

  if (die >= 0)
    exit(die);
}

void reload_bin_data() {
  if (bin_checksum(binname, GET_CONF)) {
    free_conf();
    bin_to_conf();
    fill_conf_bot();
    if (!conf.bot->localhub)
      free_conf_bots();
  }
}

