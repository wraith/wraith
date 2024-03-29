/*
 * Provide forward compat functions when built from < 1.1 in case the
 * binary is run against a 1.1+ library. It will first try to find
 * the symbol it wants, like SSLv23_client_method(), and fallback to
 * _SSLv23_client_method() for TLS_client_method() if not found.
 *
 * Each condition here should match the condition in libssl.cc/libcrypto.cc
 * for where DLSYM_GLOBAL_FWDCOMPAT() is used.
 */
#include <openssl/opensslv.h>

#include <stdlib.h>
#include <stdint.h>
#include "dl.h"

extern "C" {
/*
 * src/libssl.cc
 */
#if !defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER < 0x10100000L
typedef int (*OPENSSL_init_ssl_t)(uint64_t a1, const void *a2);
static int _OPENSSL_init_ssl(uint64_t a1, const void *a2) {
  if (DLSYM_VAR(OPENSSL_init_ssl) == NULL)
    if (DLSYM_GLOBAL_SIMPLE(RTLD_NEXT, OPENSSL_init_ssl) == NULL)
      return 0;
  return DLSYM_VAR(OPENSSL_init_ssl)(a1, a2);
}
int _SSL_library_init(void) {
  return _OPENSSL_init_ssl(0, NULL);
}

#define OPENSSL_INIT_LOAD_CRYPTO_STRINGS    0x00000002L
#define OPENSSL_INIT_LOAD_SSL_STRINGS       0x00200000L
void _SSL_load_error_strings(void) {
    _OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS \
                     | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
}
#endif

#if !((defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER > 0x20020002L) || \
    (!defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x10100000L))
typedef void *(*TLS_client_method_t)(void);
static const void *_TLS_client_method(void) {
  if (DLSYM_VAR(TLS_client_method) == NULL)
    if (DLSYM_GLOBAL_SIMPLE(RTLD_NEXT, TLS_client_method) == NULL)
      return NULL;
  return DLSYM_VAR(TLS_client_method)();
}
#endif

#if !defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER < 0x10100000L
const void *_SSLv23_client_method(void) {
  return _TLS_client_method();
}
#endif

/*
 * src/libcrypto.cc
 */
#if !((defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER >= 0x30500000L) || \
    (!defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x10100000L))
void _ERR_free_strings(void) __attribute__((const));
void _ERR_free_strings(void) {
}

void _EVP_cleanup(void) __attribute__((const));
void _EVP_cleanup(void) {
}

void _CRYPTO_cleanup_all_ex_data(void) __attribute__((const));
void _CRYPTO_cleanup_all_ex_data(void) {
}
#endif

} /* extern "C" */
