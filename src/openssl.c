/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2010 Bryan Drewery
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
 * openssl.c -- handles:
 *   libcrypto / libssl handling
 *
 */


#include "common.h"
#include "main.h"
#include "dl.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>

#include "libssl.h"
#include "libcrypto.h"

#ifdef EGG_SSL_EXT
SSL_CTX *ssl_ctx = NULL;
char	*tls_rand_file = NULL;
#endif
int     ssl_use = 0; /* kyotou */

static int seed_PRNG(void);

int init_openssl() {
  load_libcrypto();
  load_libssl();

#ifdef EGG_SSL_EXT
  /* good place to init ssl stuff */
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
  ssl_ctx = SSL_CTX_new(SSLv23_client_method());
  if (!ssl_ctx) {
    sdprintf("SSL_CTX_new() failed");
    return 1;
  }

  // Disable insecure SSLv2
  SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2);

  if (seed_PRNG()) {
    sdprintf("Wasn't able to properly seed the PRNG!");
    SSL_CTX_free(ssl_ctx);
    ssl_ctx = NULL;
    return 1;
  }
#endif

  DH1080_init();

  return 0;
}

int uninit_openssl () {
  DH1080_uninit();

#ifdef EGG_SSL_EXT
  /* cleanup mess when quiting */
  if (ssl_ctx) {
    SSL_CTX_free(ssl_ctx);
    ssl_ctx = NULL;
  }
  if (tls_rand_file)
    RAND_write_file(tls_rand_file);
#endif

  ERR_free_strings();
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();

  unload_libssl();
  unload_libcrypto();
  return 0;
}

#ifdef EGG_SSL_EXT
static int seed_PRNG(void)
{
  char stackdata[1024];
  static char rand_file[300];
  FILE *fh;

  if (RAND_status())
    return 0;     /* PRNG already good seeded */
  /* if the device '/dev/urandom' is present, OpenSSL uses it by default.
   * check if it's present, else we have to make random data ourselfs.
   */
  if ((fh = fopen("/dev/urandom", "r"))) {
    fclose(fh);
    // Try /dev/random if urandom is unavailable
    if ((fh = fopen("/dev/random", "r"))) {
      fclose(fh);
      return 0;
    }
  }
  if (RAND_file_name(rand_file, sizeof(rand_file)))
    tls_rand_file = rand_file;
  else
    return 1;
  if (!RAND_load_file(rand_file, 1024)) {
    /* no .rnd file found, create new seed */
    RAND_seed(&now, sizeof(time_t));
    RAND_seed(&conf.bot->pid, sizeof(pid_t));
    RAND_seed(stackdata, sizeof(stackdata));
  }
  if (!RAND_status())
    return 2;   /* PRNG still badly seeded */
  return 0;
}
#endif
