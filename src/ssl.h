#ifndef _SSL_H
#define _SSL_H

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>

#ifdef EGG_SSL_EXT
# ifndef EGG_SSL_INCS
#  include <openssl/ssl.h>
#  include <openssl/err.h>
#  include <openssl/rand.h>
#  define EGG_SSL_INCS 1
# endif
#endif

int load_ssl();
int unload_ssl();

#ifdef EGG_SSL_EXT
extern SSL_CTX *ssl_ctx;
extern char *tls_rand_file;
#endif
extern int ssl_use;

#endif /* !_SSL_H */
