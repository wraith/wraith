#include <openssl/opensslv.h>
/* Provide forward compat functions when built from < 1.1. */
#if !defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER < 0x10100000L
#include <stdlib.h>
#include <stdint.h>
#include "dl.h"

extern "C" {
void _ERR_free_strings(void) __attribute__((const));
void _ERR_free_strings(void) {
}

void _EVP_cleanup(void) __attribute__((const));
void _EVP_cleanup(void) {
}

void _CRYPTO_cleanup_all_ex_data(void) __attribute__((const));
void _CRYPTO_cleanup_all_ex_data(void) {
}

typedef void *(*TLS_client_method_t)(void);
static const void *_TLS_client_method(void) {
  if (DLSYM_VAR(TLS_client_method) == NULL)
    if (DLSYM_GLOBAL_SIMPLE(RTLD_NEXT, TLS_client_method) == NULL)
      return NULL;
  return DLSYM_VAR(TLS_client_method)();
}

const void *_SSLv23_client_method(void) {
  return _TLS_client_method();
}

} /* extern "C" */
#endif	/* OPENSSL_VERSION_NUMBER < 0x10100000L */
