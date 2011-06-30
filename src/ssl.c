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
 * ssl.c -- handles:
 *   libssl handling
 *
 */


#include "common.h"
#include "main.h"
#include "dl.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>

#include "ssl.h"

#ifdef EGG_SSL_EXT
SSL_CTX *ssl_ctx = NULL;
char	*tls_rand_file = NULL;
#endif
int     ssl_use = 0; /* kyotou */

static int seed_PRNG(void);

int load_ssl() {
  if (ssl_ctx) {
    return 0;
  }

#ifdef EGG_SSL_EXT
  /* good place to init ssl stuff */
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
  ssl_ctx = SSL_CTX_new(SSLv23_client_method());
  if (!ssl_ctx)
    fatal("SSL_CTX_new() failed",0);
  if (seed_PRNG())
    fatal("Wasn't able to properly seed the PRNG!",0);
#endif

  return 0;
}

int unload_ssl() {
  if (ssl_ctx) {
#ifdef EGG_SSL_EXT
    /* cleanup mess when quiting */
    if (ssl_ctx) {
      SSL_CTX_free(ssl_ctx);
      ssl_ctx = NULL;
    }
    if (tls_rand_file)
      RAND_write_file(tls_rand_file);
#endif
    return 0;
  }
  return 1;
}

#ifdef EGG_SSL_EXT
static int seed_PRNG(void)
{
  char stackdata[1024];
  static char rand_file[300];
  FILE *fh;

#if OPENSSL_VERSION_NUMBER >= 0x00905100
  if (RAND_status())
    return 0;     /* PRNG already good seeded */
#endif
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
    unsigned int c;
    c = time(NULL);
    RAND_seed(&c, sizeof(c));
    c = getpid();
    RAND_seed(&c, sizeof(c));
    RAND_seed(stackdata, sizeof(stackdata));
  }
#if OPENSSL_VERSION_NUMBER >= 0x00905100
  if (!RAND_status())
    return 2;   /* PRNG still badly seeded */
#endif
  return 0;
}
#endif



