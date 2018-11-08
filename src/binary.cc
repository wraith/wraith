/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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
#include "tandem.h"
#include "botnet.h"
#include "net.h"
#include "userrec.h"
#include <bdlib/src/Array.h>
#include <bdlib/src/AtomicFile.h>
#include <bdlib/src/String.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>

#include <libelf.h>
#include <gelf.h>

settings_t settings = {
  SETTINGS_HEADER,
  /* -- STATIC -- */
  "", "", "", "", "", "", "", "", "", "",
  /* -- DYNAMIC -- */
  "", "", "", "", "", "", "", "", "", "",
  /* -- PADDING */
  ""
};

static void edpack(settings_t *, const char *, int);
#ifdef DEBUG
static void tellconfig(settings_t *);
#endif

#define PACK_ENC 1
#define PACK_DEC 2

int checked_bin_buf = 0;

#define MMAP_READ(_map, _dest, _offset, _len)	\
  memcpy((_dest), &(_map)[(_offset)], (_len));	\
  (_offset) += (_len);

static size_t
elf_find_data_offset(int fd) {
  Elf *elf = NULL;
  GElf_Shdr shdr;
  Elf_Scn *scn = NULL;
  const char *sh_name;
  size_t shstrndx;
  size_t offset = 0;

  if (elf_version(EV_CURRENT) == EV_NONE)
    goto failure;

  if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
    goto failure;

  if (elf_kind(elf) != ELF_K_ELF)
    goto failure;

  elf_getshdrstrndx(elf, &shstrndx);

  while ((scn = elf_nextscn(elf, scn)) != NULL) {
    if (gelf_getshdr(scn, &shdr) != &shdr)
      goto failure;
    if (shdr.sh_type != SHT_PROGBITS)
      continue;
    if ((sh_name = elf_strptr(elf, shstrndx, shdr.sh_name)) == NULL)
      goto failure;
    if (strcmp(sh_name, ".data"))
      continue;
    offset = shdr.sh_offset;
    break;
  }

  goto cleanup;

failure:
  offset = 0;
#ifdef DEBUG
  printf("Elf error: %s\n", elf_errmsg(-1));
#endif

cleanup:

  if (elf != NULL)
    elf_end(elf);
  lseek(fd, 0, SEEK_SET);

  return offset;
}

static size_t
elf_find_data_mem_offset(int fd, unsigned char *map, size_t map_size,
    void *symbol_header, size_t symbol_header_len) {
  size_t data_offset;
  unsigned char *symbol_start;

  if ((data_offset = elf_find_data_offset(fd)) == 0)
    return 0;

  symbol_start = (unsigned char*)memmem(map + data_offset,
      map_size - data_offset, symbol_header, symbol_header_len);

  if (symbol_start == NULL)
    return 0;

  return symbol_start - map;
}

static char *
bin_checksum(const char *fname, int todo)
{
  MD5_CTX ctx;
  static char hash[MD5_HASH_LENGTH + 1] = "";
  unsigned char md5out[MD5_HASH_LENGTH + 1] = "";
  int fd = -1;
  size_t offset = 0, size = 0, newpos = 0;
  unsigned char *map = NULL, *outmap = NULL;

  MD5_Init(&ctx);

  ++checked_bin_buf;
 
  hash[0] = 0;

  fixmod(fname);

  fd = open(fname, O_RDONLY);
  if (fd == -1) werr(ERR_BINSTAT);
  size = lseek(fd, 0, SEEK_END);
  map = (unsigned char*) mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if ((void*)map == MAP_FAILED) {
    close(fd);
    werr(ERR_BINSTAT);
  }
  /* Find the packdata */
  if ((offset = elf_find_data_mem_offset(fd, map, size, &settings.prefix,
      PREFIXLEN)) == 0) {
    munmap(map, size);
    close(fd);
    werr(ERR_BINSTAT);
  }
#if defined(HAVE_POSIX_MADVISE)
  posix_madvise(map, size, POSIX_MADV_SEQUENTIAL);
#elif defined(HAVE_MADVISE)
  madvise(map, size, MADV_SEQUENTIAL);
#endif
  MD5_Update(&ctx, map, offset);

  /* Hash everything after the packdata too */
  MD5_Update(&ctx, &map[offset + sizeof(settings_t)], size - (offset +
      sizeof(settings_t)));

  MD5_Final(md5out, &ctx);
  btoh(md5out, MD5_DIGEST_LENGTH, hash, sizeof(hash));
  OPENSSL_cleanse(&ctx, sizeof(ctx));
  OPENSSL_cleanse(md5out, sizeof(md5out));

  if (todo == GET_CHECKSUM) {
    munmap(map, size);
    close(fd);
  } else if (todo == GET_CONF) {
    settings_t newsettings;

    /* Read the settings struct into newsettings */
    MMAP_READ(map, &newsettings, offset, sizeof(settings));

    /* Decrypt the new data */
    edpack(&newsettings, hash, PACK_DEC);
    OPENSSL_cleanse(hash, sizeof(hash));

    /* Copy over only the dynamic data, leaving the pack config static */
    memcpy(&settings.DYNAMIC_HEADER, &newsettings.DYNAMIC_HEADER, SIZE_CONF);
    OPENSSL_cleanse(&newsettings, sizeof(settings_t));

    munmap(map, size);
    close(fd);

    return ".";
  } else if (todo & WRITE_CHECKSUM) {
    bd::AtomicFile* newbin = new bd::AtomicFile();

    strlcpy(settings.hash, hash, sizeof(settings.hash));

    /* encrypt the entire struct with the hash (including hash) */
    edpack(&settings, hash, PACK_ENC);

    //Don't clear hash if requested during the write.
    if (!(todo & GET_CHECKSUM))
      OPENSSL_cleanse(hash, sizeof(hash));

    if (!newbin->open(fname, BINMOD)) {
      goto fatal;
    }
    if (ftruncate(newbin->fd(), size)) {
      goto fatal;
    }

    /* Copy everything up to this point into the new binary (including the settings header/prefix) */
    outmap = (unsigned char*) mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED,
        newbin->fd(), 0);
    if ((void*)outmap == MAP_FAILED) goto fatal;

    offset += PREFIXLEN;
    memcpy(outmap, map, offset);

    newpos = offset;

    if (todo & WRITE_PACK) {
      /* Now copy in our encrypted settings struct */
      memcpy(&outmap[newpos], &settings.hash, SIZE_PACK);
#ifdef DEBUG
      sdprintf(STR("writing pack: %zu\n"), SIZE_PACK);
#endif
    } else {
      /* Just copy the original pack data to the new binary */
      memcpy(&outmap[newpos], &map[offset], SIZE_PACK);
    }
    offset += SIZE_PACK;
    newpos += SIZE_PACK;

    if (todo & WRITE_CONF) {
      /* Copy in the encrypted conf data */
      memcpy(&outmap[newpos], &settings.DYNAMIC_HEADER, SIZE_CONF);
#ifdef DEBUG
      sdprintf(STR("writing conf: %zu\n"), SIZE_CONF);
#endif
    } else {
      /* Just copy the original conf data to the new binary */
      memcpy(&outmap[newpos], &map[offset], SIZE_CONF);
    }

    newpos += SIZE_CONF;
    offset += SIZE_CONF;

    /* Write the rest of the binary */
    memcpy(&outmap[newpos], &map[offset], size - offset);
    newpos += size - offset;
    offset += size - offset;

    munmap(map, size);
    close(fd);
    fd = -1;

    munmap(outmap, size);

    if (size != newpos) {
      delete newbin;
      fatal(STR("Binary corrupted"), 0);
    }

    if (!newbin->commit()) {
      printf(STR("Failed to commit to file %s: %s\n"), fname, strerror(errno));
      delete newbin;
      fatal("", 0);
    }

    delete newbin;
    
    return hash;
  fatal:
    if (map != NULL && (void*)map != MAP_FAILED)
      munmap(map, size);
    if (fd != -1)
      close(fd);

    if (outmap != NULL && (void*)outmap != MAP_FAILED)
      munmap(outmap, size);
    delete newbin;
    werr(ERR_BINSTAT);
  }

  return hash;
}

static int
features_find(const char *buffer)
{
  if (!md5cmp(STR("9c24e09d643af8808b3e985a8bf1f2ce"), buffer))
    return FEATURE_1;
  else if (!md5cmp(STR("5593f7d910fde28a5988e3efbdb30bbf"), buffer))
    return FEATURE_2;
  return 0;
}

/* It is desirable to use this bit on systems that have it.
   The only bit of terminal state we want to twiddle is echoing, which is
   done in software; there is no need to change the state of the terminal
   hardware.  */

#ifndef TCSASOFT
# define TCSASOFT 0
#endif

typedef struct line_list {
  struct line_list* next;
  int line;
} line_list_t;

static int
readcfg(const char *cfgfile, bool read_stdin)
{
  FILE *f = NULL;
  struct termios s, t;
  bool tty_changed = false;
  line_list_t *error_list = NULL, *error_line = NULL;

  if (read_stdin) {
    f = stdin;
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    /* Taken from libc: getpass.c */
    /* Turn echoing off if it is on now.  */
    if (tcgetattr (fileno (stdin), &t) == 0) {
      s = t;
      t.c_lflag &= ~(ECHO | ISIG);
      tty_changed = (tcsetattr (fileno (stdin), TCSAFLUSH | TCSASOFT, &t) == 0);
    }
    printf(STR("// Paste in your PACKCONFIG. Reference http://wraith.botpack.net/wiki/PackConfig\n"));
    printf(STR("// Press <enter> if it gets hung up. If that doesn't work hit ^D (CTRL+d)\n"));
    fflush(stdout);
  } else {
    if ((f = fopen(cfgfile, "r")) == NULL) {
      fprintf(stderr, STR("Error: Can't open '%s' for reading\n"), cfgfile);
      exit(1);
    }
  }

  char *buffer = NULL, *p = NULL;
  int skip = 0, line = 0, feature = 0;
  bool error = 0;
  bd::Array<bd::String> words;
  bd::String line_str;

#define ADD_ERROR(str) \
  if (line != -1) { \
    fprintf(stderr, "\n[Line %2d]: %s\n", line, str); \
    error_line = (line_list_t *) calloc(1, sizeof(line_list_t)); \
    error_line->line = line; \
    error_line->next = NULL; \
    list_append((struct list_type **) &(error_list), (struct list_type *) error_line); \
  } else \
    fprintf(stderr, "\n%s\n", str); \
  error = 1

  settings.salt1[0] = settings.salt2[0] = 0;
  if (!read_stdin) {
    printf(STR("Reading '%s' "), cfgfile);
    fflush(stdout);
  }
  while ((!feof(f)) && ((buffer = step_thru_file(f)) != NULL)) {
    if (tty_changed) {
      printf(".");
      fflush(stdout);
    }
    ++line;
    if ((*buffer)) {
      if (strchr(buffer, '\n'))
        *(char *) strchr(buffer, '\n') = 0;
      if ((skipline(buffer, &skip)))
        continue;
      if ((strchr(buffer, '<') || strchr(buffer, '>')) && !strstr(buffer, "SALT")) {
        ADD_ERROR(STR("Invalid <>"));
      }
      p = strchr(buffer, ' ');
      while (p && (strchr(LISTSEPERATORS, p[0])))
        *p++ = 0;
      if (p) {
        line_str = bd::String(trim(p));
        words = line_str.split(" ");

        if (!strcasecmp(buffer, STR("packname"))) {
          if (words.length() == 0) {
            ADD_ERROR(STR("PACKNAME requires argument"));
          }
          strlcpy(settings.packname, line_str.c_str(), sizeof settings.packname);
        } else if (!strcasecmp(buffer, STR("shellhash")) || !strcasecmp(buffer, STR("binarypass"))) {
          if (line_str.length() != 40 && line_str.length() != 47) {
            ADD_ERROR(STR("BINARYPASS should be a SHA1 hash or salted-SHA1 hash."));
          }
          strlcpy(settings.shellhash, line_str.c_str(), sizeof settings.shellhash);
        } else if (!strcasecmp(buffer, STR("dccprefix"))) {
          if (words.length() == 0) {
            ADD_ERROR(STR("DCCPREFIX requires argument"));
          }
          strlcpy(settings.dcc_prefix, line_str.c_str(), 2);
        } else if (!strcasecmp(buffer, STR("owner"))) {
          if (words.length() < 2) {
            ADD_ERROR(STR("OWNER requires 2 arguments: nick password"));
          }
          strlcat(settings.owners, line_str.c_str(), sizeof(settings.owners));
          strlcat(settings.owners, ",", sizeof(settings.owners));
        } else if (!strcasecmp(buffer, STR("hub"))) {
          if (words.length() < 3) {
            ADD_ERROR(STR("HUB requires 3 arguments: nick host port"));
          }
          strlcat(settings.hubs, line_str.c_str(), sizeof(settings.hubs));
          strlcat(settings.hubs, ",", sizeof(settings.hubs));
        } else if (!strcasecmp(buffer, STR("salt1"))) {
          if (words.length() == 0) {
            ADD_ERROR(STR("SALT1 requires argument"));
          }
          strlcat(settings.salt1, line_str.c_str(), sizeof(settings.salt1));
        } else if (!strcasecmp(buffer, STR("salt2"))) {
          if (words.length() == 0) {
            ADD_ERROR(STR("SALT2 requires argument"));
          }
          strlcat(settings.salt2, line_str.c_str(), sizeof(settings.salt2));
          if (read_stdin) break;
        }
      } else { /* SINGLE DIRECTIVES */
        if ((feature = features_find(buffer))) {
          int features = atol(settings.features);
          features |= feature;
          simple_snprintf(settings.features, sizeof(settings.features), "%d", features);
        }
      }
    }
    buffer = NULL;
  }

  if (f && !read_stdin) {
    fclose(f);
  } else if (read_stdin && tty_changed) {
    /* Restore the original setting.  */
    tcsetattr (fileno (stdin), TCSAFLUSH | TCSASOFT, &s);
  }

  line = -1;
  /* Was the entire pack read in? */
  if (!settings.packname[0]) {
    ADD_ERROR(STR("Missing PACKNAME"));
  }
  if (!settings.dcc_prefix[0]) {
    ADD_ERROR(STR("Missing DCCPREFIX"));
  }
  if (!settings.shellhash[0]) {
    ADD_ERROR(STR("Missing BINARYPASS"));
  }
  if (!settings.owners[0]) {
    ADD_ERROR(STR("Missing OWNER"));
  }
  if (!settings.hubs[0]) {
    ADD_ERROR(STR("Missing HUBS"));
  }
  if (!settings.salt1[0] || !settings.salt2[0]) {
    ADD_ERROR(STR("Missing SALTS"));
  }


  if (error) {
    printf("\n");
    fprintf(stderr, STR("Error: Look at your configuration again and fix any errors.\n"));
    for (error_line = error_list; error_line; error_line = error_line->next)
        fprintf(stderr, STR("Line %d\n"), error_line->line);
    exit(1);
  }

  if (!read_stdin) printf(STR(" Success\n"));
  else printf("\n");
  return 1;
}

void writecfg() {
  const char salt1[] = SALT1;
  const char salt2[] = SALT2;
  const auto& owners(bd::String(settings.owners).split(","));

  printf("PACKNAME %s\n", settings.packname);
  // Display the shellhash as salted-sha1 if it is not already
  const bd::String shellhash(settings.shellhash);
  printf("BINARYPASS %s\n", (shellhash.length() == 40 || shellhash.length() == 47) ? shellhash.c_str() : salted_sha1(shellhash.c_str()));
  printf("DCCPREFIX %s\n", settings.dcc_prefix);
  for (const auto &ownerLine : owners) {
    auto ownerInfo(ownerLine.split(" "));
    // Ensure the pass is salted-sha1
    const size_t ownerPassLength = ownerInfo.at(1).length();
    if (ownerPassLength != 40 && ownerPassLength != 47) {
      ownerInfo[1] = bd::String(salted_sha1(ownerInfo.at(1).c_str()));
    }
    printf("OWNER %s\n", ownerInfo.join(" ").c_str());
  }
  for (auto hubLine : conf.hubs) {
    auto hubInfo = hubLine.split(' ');
    // Trim away hublevel
    if (hubInfo.length() == 4) {
      hubInfo.resize(3);
    }
    printf("HUB %s\n", hubInfo.join(" ").c_str());
  }
  printf("SALT1 %s\n", salt1);
  printf("SALT2 %s\n", salt2);
}

static void edpack(settings_t *incfg, const char *in_hash, int what)
{
  char *tmp = NULL, *hash = (char *) in_hash, nhash[51] = "";
  unsigned char *(*enc_dec_string)(const char *, unsigned char *, size_t *);
  size_t len = 0;

  if (what == PACK_ENC)
    enc_dec_string = aes_encrypt_ecb_binary;
  else
    enc_dec_string = aes_decrypt_ecb_binary;

#define dofield(_field) 		do {				\
    len = sizeof(_field) - 1;						\
    tmp = (char *) enc_dec_string(hash, (unsigned char *) _field, &len);\
    if (what == PACK_ENC) 						\
      memcpy(_field, tmp, len);						\
    else 								\
      simple_snprintf(_field, sizeof(_field), "%s", tmp);		\
    OPENSSL_cleanse(tmp, len);						\
    free(tmp);								\
} while (0)

#define dohash(_field)		do {					\
	if (what == PACK_ENC)						\
	  strlcat(nhash, _field, sizeof(nhash));			\
	dofield(_field);						\
	if (what == PACK_DEC)						\
	  strlcat(nhash, _field, sizeof(nhash));			\
} while (0)

#define update_hash()		do {					\
	MD5(NULL);							\
	hash = MD5(nhash);						\
	OPENSSL_cleanse(nhash, sizeof(nhash));				\
	nhash[0] = 0;							\
} while (0)

  /* -- STATIC -- */

  dohash(incfg->hash);
  dohash(incfg->packname);
  update_hash();

  dohash(incfg->shellhash);
  update_hash();

  dofield(incfg->dcc_prefix);
  dofield(incfg->features);
  dofield(incfg->owners);
  dofield(incfg->hubs);

  dohash(incfg->salt1);
  dohash(incfg->salt2);
  update_hash();

  /* -- DYNAMIC -- */
  dofield(incfg->dynamic_initialized);
  dofield(incfg->conf_hubs);
  dofield(incfg->bots);
  dofield(incfg->uid);
  dofield(incfg->autocron);
  dofield(incfg->username);
  dofield(incfg->datadir);
  dofield(incfg->homedir);
  dofield(incfg->portmin);
  dofield(incfg->portmax);


  OPENSSL_cleanse(nhash, sizeof(nhash));
#undef dofield
#undef dohash
#undef update_hash
}

#ifdef DEBUG 
static void
tellconfig(settings_t *incfg)
{
#define dofield(_field)		printf("%s: %s\n", #_field, _field);
  // -- STATIC --
  dofield(incfg->hash);
  dofield(incfg->packname);
  dofield(incfg->shellhash);
  dofield(incfg->dcc_prefix);
  dofield(incfg->features);
  dofield(incfg->owners);
  dofield(incfg->hubs);
//  dofield(incfg->salt1);
//  dofield(incfg->salt2);
  // -- DYNAMIC --
  dofield(incfg->dynamic_initialized);
  dofield(incfg->conf_hubs);
  dofield(incfg->bots);
  dofield(incfg->uid);
  dofield(incfg->autocron);
  dofield(incfg->username);
  dofield(incfg->datadir);
  dofield(incfg->homedir);
  dofield(incfg->portmin);
  dofield(incfg->portmax);
#undef dofield
}
#endif

void
check_sum(const char *fname, const char *cfgfile, bool read_stdin)
{
   if (!settings_initialized()) {

    if (!cfgfile && !read_stdin)
      fatal(STR("Binary not initialized."), 0);

    readcfg(cfgfile, read_stdin);

// tellconfig(&settings); 
    if (bin_checksum(fname, WRITE_CHECKSUM|WRITE_CONF|WRITE_PACK))
      printf(STR("* Wrote settings to binary.\n")); 
    exit(0);
  } else {
    if (cfgfile || read_stdin)
      werr(ERR_ALREADYINIT);
    char *hash = bin_checksum(fname, GET_CHECKSUM);

// tellconfig(&settings); 
    edpack(&settings, hash, PACK_DEC);

    INIT_SALTS;
    OPENSSL_cleanse(settings.salt1, sizeof(settings.salt1));
    OPENSSL_cleanse(settings.salt2, sizeof(settings.salt2));
#ifdef DEBUG
 tellconfig(&settings); 
#endif
    int n = strcmp(settings.hash, hash);
    OPENSSL_cleanse(settings.hash, sizeof(settings.hash));
    OPENSSL_cleanse(hash, strlen(hash));

    if (n) {
      OPENSSL_cleanse(&settings, sizeof(settings_t));
      CLEAR_SALTS;
    }
  }
}

/*
 * @returns 0 = initialized, 1 = not initialized, 2 = corrupted
 */
int check_bin_initialized(const char *fname)
{
  const char* argv[] = {fname, "-p", 0};
  int i = simple_exec(argv);
  if (i != -1) {
    if (WEXITSTATUS(i) == 4)
      return 0;
    else if (WEXITSTATUS(i) == 5)
      return 1;
  }

  return 2;
}

bool check_bin_compat(const char *fname)
{
  size_t len = strlen(shell_escape(fname)) + 3 + 1;
  char *path = (char *) calloc(1, len);

  char *out = NULL;

  simple_snprintf(path, len, STR("%s -3"), shell_escape(fname));
  if (shell_exec(path, NULL, &out, NULL, 1)) {
    if (out) {
      char *buf = out;
      size_t settings_ver = atoi(newsplit(&buf)), settings_len = atoi(newsplit(&buf));
      if (settings_ver == SETTINGS_VER && settings_len == sizeof(settings_t)) {
        free(path);
        free(out);
        return 1;
      }
      free(out);
    }
  }
  free(path);
  return 0;
}

void write_settings(const char *fname, int die, bool doconf, int initialized)
{
  char *hash = NULL;
  int bits = WRITE_CHECKSUM;
  /* see if the binary is already initialized or not */
  if (initialized == -1)
    initialized = check_bin_initialized(fname) ? 0 : 1;

  /* only write pack data if the binary is uninitialized
   * otherwise, assume it has similar/correct/updated pack data
   */
  if (!initialized)
    bits |= WRITE_PACK;
  if (doconf)
    bits |= WRITE_CONF;

  /* only bother writing anything if we have pack or doconf, checksum is worthless to write out */
  if (bits & (WRITE_PACK|WRITE_CONF)) {

    // Also get the checksum
    if (die == -1)
      bits |= GET_CHECKSUM;

    const char salt1[] = SALT1;
    const char salt2[] = SALT2;
    strlcpy(settings.salt1, salt1, sizeof(settings.salt1));
    strlcpy(settings.salt2, salt2, sizeof(settings.salt2));

    if ((hash = bin_checksum(fname, bits))) {
      printf(STR("* Wrote %ssettings to: %s.\n"), ((bits & WRITE_PACK) && !(bits & WRITE_CONF)) ? "pack " :
                                             ((bits & WRITE_CONF) && !(bits & WRITE_PACK)) ? "conf " :
                                             ((bits & WRITE_PACK) && (bits & WRITE_CONF))  ? "pack/conf "  :
                                             "",
                                             fname);
      if (die == -1) {			/* only bother decrypting if we aren't about to exit */
        edpack(&settings, hash, PACK_DEC);
        INIT_SALTS;
        OPENSSL_cleanse(hash, strlen(hash));
      }
    }
    if (die == -1) {
      OPENSSL_cleanse(settings.salt1, sizeof(settings.salt1));
      OPENSSL_cleanse(settings.salt2, sizeof(settings.salt2));
    }
  }

  if (die >= 0)
    exit(die);
}

static void 
clear_settings(void)
{
//  memset(&settings.bots, 0, sizeof(settings_t) - SIZE_PACK - PREFIXLEN);
  memset(&settings.DYNAMIC_HEADER, 0, SIZE_CONF);
}

void conf_to_bin(conf_t *in, bool move, int die)
{
  conf_bot *bot = NULL;
  char *newbin = NULL;

  clear_settings();
  sdprintf("converting conf to bin\n");
  simple_snprintf(settings.uid, sizeof(settings.uid), "%d", in->uid);
  strlcpy(settings.dynamic_initialized, "1", sizeof(settings.dynamic_initialized));
  simple_snprintf(settings.autocron, sizeof(settings.autocron), "%d", in->autocron);
  simple_snprintf(settings.portmin, sizeof(settings.portmin), "%d", in->portmin);
  simple_snprintf(settings.portmax, sizeof(settings.portmax), "%d", in->portmax);

  if (in->username)
    strlcpy(settings.username, in->username, sizeof(settings.username));
  strlcpy(settings.datadir, in->datadir, sizeof(settings.datadir));
  if (in->homedir)
    strlcpy(settings.homedir, in->homedir, sizeof(settings.homedir));
  for (bot = in->bots; bot && bot->nick; bot = bot->next) {
    simple_snprintf(settings.bots, sizeof(settings.bots), STR("%s%s%s %s %s%s %s,"), 
                           settings.bots[0] ? settings.bots : "",
                           bot->disabled ? "/" : "",
                           bot->nick,
                           bot->net.ip ? bot->net.ip : "*", 
                           bot->net.host6 ? "+" : "", 
                           bot->net.host ? bot->net.host : (bot->net.host6 ? bot->net.host6 : "*"),
                           bot->net.ip6 ? bot->net.ip6 : "");
    }

  simple_snprintf(settings.conf_hubs, sizeof(settings.conf_hubs), in->hubs.join(',').c_str());

  newbin = binname;
//  tellconfig(&settings); 
  write_settings(newbin, -1, 1);

  if (die >= 0)
    exit(die);
}

void reload_bin_data() {
  if (bin_checksum(binname, GET_CONF)) {
    putlog(LOG_MISC, "*", STR("Rehashed config data from binary."));

    conf_bot *oldbots = NULL, *oldbot = NULL;
    bool was_localhub = conf.bot->localhub ? 1 : 0;
    
    /* save the old bots list */
    if (conf.bots)
      oldbots = conf_bots_dup(conf.bots);

    /* Save the old conf.bot */
    oldbot = (conf_bot *) calloc(1, sizeof(conf_bot));
    conf_bot_dup(oldbot, conf.bot);

    /* free up our current conf struct */
    free_conf();

    /* Fill conf[] with binary data from settings[] */
    bin_to_conf();

    /* fill up conf.bot using origbotname */
    fill_conf_bot(0); /* 0 to avoid exiting if conf.bot cannot be filled */

    if (was_localhub) {
      /* add any new bots not in userfile */
      conf_add_userlist_bots();

       /* deluser removed bots from conf */
      if (oldbots)
        deluser_removed_bots(oldbots, conf.bots);
    }

    if (conf.bot && conf.bot->disabled) {
      if (tands > 0) {
        botnet_send_chat(-1, conf.bot->nick, STR("Bot disabled in binary."));
        botnet_send_bye(STR("Bot disabled in binary."));
      }

      if (server_online)
        nuke_server(STR("bbl"));

      werr(ERR_BOTDISABLED);
    } else if (!conf.bot) {
      conf.bot = oldbot;

      if (tands > 0) {
        botnet_send_chat(-1, conf.bot->nick, STR("Bot removed from binary."));
        botnet_send_bye(STR("Bot removed from binary."));
      }

      if (server_online)
        nuke_server(STR("it's been good, cya"));

      werr(ERR_BADBOT);
    }

    /* The bot nick changed! (case) */
    if (strcmp(conf.bot->nick, oldbot->nick)) {
      change_handle(conf.bot->u, conf.bot->nick);
//      var_set_by_name(conf.bot->nick, "nick", conf.bot->nick);
//      var_set_userentry(conf.bot->nick, "nick", conf.bot->nick);
    }

    free_bot(oldbot);

    if (oldbots)
      free_conf_bots(oldbots);

    if (!conf.bot->localhub && !conf.bot->hub) {
      free_conf_bots(conf.bots);

      if (was_localhub) {
        //Close the listening port
        for (int i = 0; i < dcc_total; i++) {
          if (dcc[i].type && (dcc[i].type == &DCC_TELNET) && (strchr(dcc[i].host, '/'))) {
              unlink(dcc[i].host);
              killsock(dcc[i].sock);
              lostdcc(i);
          }
        }
      }
    }
  }
}

/* vim: set sts=2 sw=2 ts=8 et: */
