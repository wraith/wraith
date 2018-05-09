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
 * libcrypto.c -- handles:
 *   libcrypto handling
 *
 */


#include "common.h"
#include "main.h"
#include "dl.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>

#include "libcrypto.h"
#include ".defs/libcrypto_defs.cc"

void *libcrypto_handle = NULL;
static bd::Array<bd::String> my_symbols;

static int load_symbols(void *handle) {
  DLSYM_GLOBAL(handle, AES_cbc_encrypt);
  DLSYM_GLOBAL(handle, AES_decrypt);
  DLSYM_GLOBAL(handle, AES_encrypt);
  DLSYM_GLOBAL(handle, AES_set_decrypt_key);
  DLSYM_GLOBAL(handle, AES_set_encrypt_key);
  DLSYM_GLOBAL(handle, BF_decrypt);
  DLSYM_GLOBAL(handle, BF_encrypt);
  DLSYM_GLOBAL(handle, BF_set_key);
  DLSYM_GLOBAL(handle, ERR_error_string);
  DLSYM_GLOBAL(handle, ERR_get_error);
  DLSYM_GLOBAL(handle, OPENSSL_cleanse);
  DLSYM_GLOBAL(handle, RAND_file_name);
  DLSYM_GLOBAL(handle, RAND_load_file);
  DLSYM_GLOBAL(handle, RAND_seed);
  DLSYM_GLOBAL(handle, RAND_status);
  DLSYM_GLOBAL(handle, RAND_write_file);
  DLSYM_GLOBAL(handle, MD5_Final);
  DLSYM_GLOBAL(handle, MD5_Init);
  DLSYM_GLOBAL(handle, MD5_Update);
  DLSYM_GLOBAL(handle, SHA1_Final);
  DLSYM_GLOBAL(handle, SHA1_Init);
  DLSYM_GLOBAL(handle, SHA1_Update);
  DLSYM_GLOBAL(handle, SHA256_Final);
  DLSYM_GLOBAL(handle, SHA256_Init);
  DLSYM_GLOBAL(handle, SHA256_Update);

  DLSYM_GLOBAL(handle, BN_bin2bn);
  DLSYM_GLOBAL(handle, BN_bn2bin);
  DLSYM_GLOBAL(handle, BN_clear_free);
  DLSYM_GLOBAL(handle, BN_dec2bn);
  DLSYM_GLOBAL(handle, BN_dup);
  DLSYM_GLOBAL(handle, BN_hex2bn);
  DLSYM_GLOBAL(handle, BN_num_bits);

  DLSYM_GLOBAL(handle, DH_compute_key);
  DLSYM_GLOBAL(handle, DH_free);
  DLSYM_GLOBAL(handle, DH_generate_key);
  DLSYM_GLOBAL(handle, DH_new);
  DLSYM_GLOBAL(handle, DH_size);

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  /* For dh_util.cc */
  DLSYM_GLOBAL(handle, DH_get0_key);
  DLSYM_GLOBAL(handle, DH_set0_key);
  DLSYM_GLOBAL(handle, DH_set0_pqg);
  DLSYM_GLOBAL(handle, BN_free);
#else
  DLSYM_GLOBAL_FWDCOMPAT(handle, ERR_free_strings);
  DLSYM_GLOBAL_FWDCOMPAT(handle, EVP_cleanup);
  DLSYM_GLOBAL_FWDCOMPAT(handle, CRYPTO_cleanup_all_ex_data);
#endif

  return 0;
}


int load_libcrypto() {
  if (libcrypto_handle) {
    return 0;
  }

  sdprintf("Loading libcrypto");

  bd::Array<bd::String> libs_list(bd::String("libcrypto.so." SHLIB_VERSION_NUMBER " libcrypto.so libcrypto.so.1.1 libcrypto.so.1.0.0 libcrypto.so.0.9.8 libcrypto.so.10 libcrypto.so.9 libcrypto.so.8 libcrypto.so.7 libcrypto.so.6").split(' '));

  for (size_t i = 0; i < libs_list.length(); ++i) {
    dlerror(); // Clear Errors
    libcrypto_handle = dlopen(bd::String(libs_list[i]).c_str(), RTLD_LAZY|RTLD_GLOBAL);
    if (libcrypto_handle) {
      sdprintf("Found libcrypto: %s", bd::String(libs_list[i]).c_str());
      break;
    }
  }
  if (!libcrypto_handle) {
    fprintf(stderr, STR("Unable to find libcrypto\n"));
    return(1);
  }

  if (load_symbols(libcrypto_handle)) {
    fprintf(stderr, STR("Missing symbols for libcrypto (likely too old)\n"));
    return(1);
  }

  return 0;
}

int unload_libcrypto() {
  if (libcrypto_handle) {
    // Cleanup symbol table
    for (size_t i = 0; i < my_symbols.length(); ++i) {
      dl_symbol_table.remove(my_symbols[i]);
      static_cast<bd::String>(my_symbols[i]).clear();
    }
    my_symbols.clear();

    dlclose(libcrypto_handle);
    libcrypto_handle = NULL;
    return 0;
  }
  return 1;
}
/* vim: set sts=2 sw=2 ts=8 et: */
