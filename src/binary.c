/*
 * binary.c -- handles:
 *   misc update functions
 *   md5 hash verifying
 *
 */

#include "common.h"
#include "binary.h"
#include "crypt.h"
#include "shell.h"
#include "main.h"
#include "salt.h"
#include "misc_file.h"

#ifndef CYGWIN_HACKS

typedef struct encdata_struct {
  char prefix[16];
  char data[512];
} encdata_t;

static encdata_t encdata = {
  "AAAAAAAAAAAAAAAA",
  ""
};

int checked_bin_buf = 0;

static char *
bin_md5(const char *fname, int todo)
{
  static char hash[MD5_HASH_LENGTH + 1] = "";
  unsigned char md5out[MD5_HASH_LENGTH + 1] = "", buf[17] = "";
  FILE *f = NULL;
  size_t len = 0;
  MD5_CTX ctx;

  checked_bin_buf++;
  if (!(f = fopen(fname, "rb")))
    werr(ERR_BINSTAT);

  MD5_Init(&ctx);
  while ((len = fread(buf, 1, sizeof buf - 1, f))) {
    if (!memcmp(buf, &encdata.prefix, 16)) {
      break;
    }
    MD5_Update(&ctx, buf, len);
  }

  fclose(f);
  MD5_Final(md5out, &ctx);
  strncpyz(hash, btoh(md5out, MD5_DIGEST_LENGTH), sizeof(hash));
  OPENSSL_cleanse(&ctx, sizeof(ctx));

  if (todo == WRITE_MD5) {
    char *fname_bak = NULL, s[DIRMAX] = "";
    FILE *fn = NULL;            /* the new binary */
    int i = 0, fd = -1;
    size_t size = 0;


    size = strlen(fname) + 2;
    fname_bak = calloc(1, size);
    egg_snprintf(fname_bak, size, "%s~", fname);
    size = 0;

    if (!(f = fopen(fname, "rb")))
      werr(ERR_BINSTAT);

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    egg_snprintf(s, sizeof s, ".bin-XXXXXX");
    if ((fd = mkstemp(s)) == -1 || (fn = fdopen(fd, "wb")) == NULL) {
      if (fd != -1) {
        unlink(s);
        close(fd);
      }
      fatal("Can't create temporary file!", 0);
    }

    while ((len = fread(buf, 1, sizeof(buf) - 1, f))) {
      if (i) {                  /* to skip bytes for hash */
        i -= sizeof(buf) - 1;
        continue;
      }
      if (fwrite(buf, sizeof(buf) - 1, 1, fn) != 1) {
        fclose(f);
        fclose(fn);
        unlink(s);
        werr(ERR_BINSTAT);
      }

      if (!memcmp(buf, &encdata.prefix, 16)) {
        /* now we have 512 for data :D */
        char *enc_hash = NULL;

        enc_hash = encrypt_string(SALT1, hash);
        fwrite(enc_hash, strlen(enc_hash), 1, fn);
        i = strlen(enc_hash);   /* skip the next strlen(enc_hash) bytes */
        free(enc_hash);
      }
    }

    fclose(f);
    fclose(fn);

    if (movefile(fname, fname_bak))
      fatal("Crappy os :D", 0);

    if (movefile(s, fname))
      fatal("Crappy os :D", 0);

    fixmod(fname);
    unlink(fname_bak);
    unlink(s);
  }

  return hash;
}


void
check_sum(const char *fname)
{
  if (!encdata.data[0]) {
    printf("* Wrote checksum to binary. (%s)\n", bin_md5(fname, WRITE_MD5));
  } else {
    char *hash = NULL;

    hash = decrypt_string(SALT1, encdata.data);

    if (strcmp(bin_md5(fname, GET_MD5), hash)) {
      free(hash);
      unlink(fname);
      fatal("!! Invalid binary", 0);
    }
    free(hash);
  }
}
#endif /* !CYGWIN_HACKS */
