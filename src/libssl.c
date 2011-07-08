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
 * libssl.c -- handles:
 *   libssl handling
 *
 */


#include "common.h"
#include "main.h"
#include "dl.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>

#include "libssl.h"
#include ".defs/libssl_defs.c"

void *libssl_handle = NULL;
static bd::Array<bd::String> my_symbols;
#ifdef EGG_SSL_EXT
SSL_CTX *ssl_ctx = NULL;
char	*tls_rand_file = NULL;
#endif
int     ssl_use = 0; /* kyotou */

static int seed_PRNG(void);

static int load_symbols(void *handle) {
  const char *dlsym_error = NULL;

  DLSYM_GLOBAL(handle, SSL_get_error);
  DLSYM_GLOBAL(handle, SSL_connect);
  DLSYM_GLOBAL(handle, SSL_CTX_free);
  DLSYM_GLOBAL(handle, SSL_CTX_new);
  DLSYM_GLOBAL(handle, SSL_free);
  DLSYM_GLOBAL(handle, SSL_library_init);
  DLSYM_GLOBAL(handle, SSL_load_error_strings);
  DLSYM_GLOBAL(handle, SSL_new);
  DLSYM_GLOBAL(handle, SSL_pending);
  DLSYM_GLOBAL(handle, SSL_read);
  DLSYM_GLOBAL(handle, SSL_set_fd);
  DLSYM_GLOBAL(handle, SSL_shutdown);
  DLSYM_GLOBAL(handle, SSLv23_client_method);
  DLSYM_GLOBAL(handle, SSL_write);
  DLSYM_GLOBAL(handle, SSL_CTX_ctrl);

  return 0;
}


int load_ssl() {
  if (ssl_ctx) {
    return 0;
  }

  sdprintf("Loading libssl");

  bd::Array<bd::String> libs_list(bd::String("libssl.so libssl.so.0.9.8 libssl.so.7 libssl.so.6").split(' '));

  for (size_t i = 0; i < libs_list.length(); ++i) {
    dlerror(); // Clear Errors
    libssl_handle = dlopen(bd::String(libs_list[i]).c_str(), RTLD_LAZY);
    if (libssl_handle) {
      sdprintf("Found libssl: %s", bd::String(libs_list[i]).c_str());
      break;
    }
  }
  if (!libssl_handle) {
    sdprintf("Unable to find libssl");
    return(1);
  }

  load_symbols(libssl_handle);

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
    return 1;
  }
#endif

  return 0;
}

int unload_ssl() {
  if (libssl_handle) {
#ifdef EGG_SSL_EXT
    /* cleanup mess when quiting */
    if (ssl_ctx) {
      SSL_CTX_free(ssl_ctx);
      ssl_ctx = NULL;
    }
    if (tls_rand_file)
      RAND_write_file(tls_rand_file);
#endif

    // Cleanup symbol table
    for (size_t i = 0; i < my_symbols.length(); ++i) {
      dl_symbol_table.remove(my_symbols[i]);
      static_cast<bd::String>(my_symbols[i]).clear();
    }
    my_symbols.clear();

    dlclose(libssl_handle);
    libssl_handle = NULL;
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



