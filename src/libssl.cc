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
#include ".defs/libssl_defs.cc"

void *libssl_handle = NULL;
static bd::Array<bd::String> my_symbols;

static int load_symbols(void *handle) {
  DLSYM_GLOBAL(handle, SSL_get_error);
  DLSYM_GLOBAL(handle, SSL_connect);
  DLSYM_GLOBAL(handle, SSL_CTX_free);
  DLSYM_GLOBAL(handle, SSL_CTX_new);
  DLSYM_GLOBAL(handle, SSL_free);
  DLSYM_GLOBAL(handle, SSL_new);
  DLSYM_GLOBAL(handle, SSL_pending);
  DLSYM_GLOBAL(handle, SSL_read);
  DLSYM_GLOBAL(handle, SSL_set_fd);
  DLSYM_GLOBAL(handle, SSL_shutdown);
  DLSYM_GLOBAL(handle, SSL_write);
  DLSYM_GLOBAL(handle, SSL_CTX_ctrl);
  DLSYM_GLOBAL(handle, SSL_CTX_set_cipher_list);
  DLSYM_GLOBAL(handle, SSL_CTX_set_tmp_dh_callback);
#if defined(OPENSSL_API_COMPAT) && OPENSSL_API_COMPAT < 0x10100000L
  /* For SSL_library_init and SSL_load_error_strings. */
  DLSYM_GLOBAL(handle, OPENSSL_init_ssl);
#else
  DLSYM_GLOBAL_FWDCOMPAT(handle, SSL_library_init);
  DLSYM_GLOBAL_FWDCOMPAT(handle, SSL_load_error_strings);
  /* Some forward-compat is handled in src/compat/openssl.cc. */
#endif
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  /* For SSLv23_client_method */
  DLSYM_GLOBAL(handle, TLS_client_method);
  /* For SSLv23_method */
  DLSYM_GLOBAL(handle, TLS_method);
  DLSYM_GLOBAL(handle, SSL_CTX_set_options);
#else
  DLSYM_GLOBAL_FWDCOMPAT(handle, SSLv23_client_method);
  /* Some forward-compat is handled in src/compat/openssl.cc. */
#endif

  return 0;
}


int load_libssl() {
  if (ssl_ctx) {
    return 0;
  }

  sdprintf("Loading libssl");

  bd::Array<bd::String> libs_list(bd::String("libssl.so." SHLIB_VERSION_NUMBER " libssl.so libssl.so.1.1 libssl.so.1.0.0 libssl.so.0.9.8 libssl.so.10 libssl.so.9 libssl.so.8 libssl.so.7 libssl.so.6").split(' '));

  for (size_t i = 0; i < libs_list.length(); ++i) {
    dlerror(); // Clear Errors
    libssl_handle = dlopen(bd::String(libs_list[i]).c_str(), RTLD_LAZY);
    if (libssl_handle) {
      sdprintf("Found libssl: %s", bd::String(libs_list[i]).c_str());
      break;
    }
  }
  if (!libssl_handle) {
    fprintf(stderr, STR("Unable to find libssl\n"));
    return(1);
  }

  if (load_symbols(libssl_handle)) {
    fprintf(stderr, STR("Missing symbols for libssl (likely too old)\n"));
    return(1);
  }

  return 0;
}

int unload_libssl() {
  if (libssl_handle) {
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
/* vim: set sts=2 sw=2 ts=8 et: */
