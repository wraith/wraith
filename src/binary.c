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

encdata_t encdata = {
  "AAAAAAAAAAAAAAAA",
  ""
};

char *
bin_md5(const char *fname, int todo)
{
  static char hash[MD5_HASH_LENGTH + 1] = "";
  unsigned char md5out[MD5_HASH_LENGTH + 1] = "";
  char *buf = NULL, *p = NULL, *fname_bak = NULL;
  FILE *f = NULL;
  size_t size = 0, size_p = 0;
  MD5_CTX ctx;

  if (!(f = fopen(fname, "rb")))
    werr(ERR_BINSTAT);

  MD5_Init(&ctx);

  size = strlen(fname) + 2;
  fname_bak = calloc(1, size);
  egg_snprintf(fname_bak, size, "%s~", fname);
  size = 0;

  fseek(f, 0, SEEK_END);
  size = ftell(f);
  fseek(f, 0, SEEK_SET);

  buf = calloc(1, size + 1);

  if (fread(buf, 1, size, f) != size)
    fatal("Can't read binary", 0);

  p = buf;
  while (p < (buf + size - 4)) {
    if (!strncmp(p, STR("AAAAAAAA"), 8))        /* this STR() is *REQUIRED* */
      break;
    p += 4;
    size_p += 4;
  }

  if (p >= (buf + size - 4))
    fatal("Shit out of luck brotha", 0);

  p += 16;
  size_p += 16;
  /* now we have 4096 for data :D */

  MD5_Update(&ctx, buf, size_p);
  MD5_Final(md5out, &ctx);
  strncpyz(hash, btoh(md5out, MD5_DIGEST_LENGTH), sizeof(hash));
  OPENSSL_cleanse(&ctx, sizeof(ctx));

  if (todo == WRITE_MD5) {
    char *enc_hash = NULL;

    enc_hash = encrypt_string(SALT1, hash);
    strncpyz(p, enc_hash, strlen(enc_hash) + 1);
    size += strlen(enc_hash);
    free(enc_hash);
    fclose(f);

    movefile(fname, fname_bak);

    if (!(f = fopen(fname, "wb"))) {
      movefile(fname_bak, fname);
      werr(ERR_BINSTAT);
    }

    if (fwrite(buf, 1, size, f) != size) {
      movefile(fname_bak, fname);
      fatal("Failed to re-write binary", 0);
    }
    fclose(f);
    fixmod(fname);
    unlink(fname_bak);
  }

  free(buf);
  return hash;
}
