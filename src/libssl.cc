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


#include "buildinfo.h"
#include "common.h"
#include "main.h"
#include "dl.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>

#include "libssl.h"
#ifndef GENERATING_DEFS
#include ".defs/libssl_defs.cc"
#endif

#ifndef OPENSSL_SHLIB_VERSION
#define OPENSSL_SHLIB_VERSION_STR SHLIB_VERSION_NUMBER
#else
#define OPENSSL_SHLIB_VERSION_STR __XSTRING(OPENSSL_SHLIB_VERSION)
#endif

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
#if defined(LIBRESSL_VERSION_NUMBER)
  /* SSL_library_init: always symbol in LibreSSL. */
  DLSYM_GLOBAL(handle, SSL_library_init);
  /* SSL_load_error_strings: always symbol in LibreSSL. */
  DLSYM_GLOBAL(handle, SSL_load_error_strings);
#elif OPENSSL_VERSION_NUMBER < 0x10100000L
  /* SSL_library_init: symbol in LibreSSL and 1.0. Macro in 1.1+. */
  DLSYM_GLOBAL_FWDCOMPAT(handle, SSL_library_init);
  /* SSL_load_error_strings: Symbol in LibreSSL and 1.0. Macro in 1.1+. */
  DLSYM_GLOBAL_FWDCOMPAT(handle, SSL_load_error_strings);
  /* Some forward-compat is handled in src/compat/openssl.cc. */
#else
  /* For SSL_library_init and SSL_load_error_strings. */
  DLSYM_GLOBAL(handle, OPENSSL_init_ssl);
#endif
#if !defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x10100000L
  /* Macro in 1.0 and LibreSSL. Symbol in 1.1+. */
  DLSYM_GLOBAL(handle, SSL_CTX_set_options);
#endif
#if (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER > 0x20020002L) || \
    (!defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x10100000L)
  /* Not in 1.0. Symbol in 1.1+ and LibreSSL 2.2+ 0x20020002L. */
  DLSYM_GLOBAL(handle, TLS_client_method);
#elif defined(LIBRESSL_VERSION_NUMBER)
  /* LibreSSL always has a symbol and not a macro to TLS_client_method. */
  DLSYM_GLOBAL(handle, SSLv23_client_method);
#else
  /* Symbol in 1.0 and LibreSSL. Macro in OpenSSL 1.1+. */
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

  const auto& libs_list(bd::String(
#if !defined(LIBRESSL_VERSION_NUMBER)
      SSL_LIBDIR "/libssl.so." OPENSSL_SHLIB_VERSION_STR " "
      "libssl.so." OPENSSL_SHLIB_VERSION_STR " "
#endif
      SSL_LIBDIR "/libssl.so "
      "libssl.so "
      "libssl.so.12 "
      "libssl.so.30 "
      "libssl.so.111 "
      "libssl.so.1.1 "
      "libssl.so.11 "
      "libssl.so.1.0.0 "
      "libssl.so.10 "
      "libssl.so.9 "
      "libssl.so.8 "
      "libssl.so.7 "
      "libssl.so.6 "
      "libssl.so.0.9.8").split(' '));

  for (const auto& lib : libs_list) {
    dlerror(); // Clear Errors
    libssl_handle = dlopen(lib.c_str(), RTLD_LAZY);
    if (libssl_handle) {
      sdprintf("Found libssl: %s", lib.c_str());
      break;
    }
  }
  if (!libssl_handle) {
    fprintf(stderr, STR("Unable to find libssl\n"));
    return(1);
  }

  if (load_symbols(libssl_handle)) {
    fprintf(stderr, STR("\nMissing symbols for libssl (likely too old)\n"));
    return(1);
  }

  return 0;
}

int unload_libssl() {
  if (libssl_handle) {
    // Cleanup symbol table
    for (const auto& symbol : my_symbols) {
      dl_symbol_table.remove(symbol);
    }
    my_symbols.clear();

    dlclose(libssl_handle);
    libssl_handle = NULL;
    return 0;
  }
  return 1;
}
/* vim: set sts=2 sw=2 ts=8 et: */
