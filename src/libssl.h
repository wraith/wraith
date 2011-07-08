#ifndef _LIBSSL_H
#define _LIBSSL_H

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>

#include ".defs/libssl_pre.h"

#ifdef EGG_SSL_EXT
# ifndef EGG_SSL_INCS
#  include <openssl/ssl.h>
#  include <openssl/err.h>
#  include <openssl/rand.h>
#  define EGG_SSL_INCS 1
# endif
#endif

#include ".defs/libssl_post.h"

typedef int (*SSL_get_error_t)(const SSL*, int);
typedef void (*SSL_free_t)(SSL*);
typedef int (*SSL_connect_t)(SSL*);
typedef int (*SSL_read_t)(SSL*, void*, int);
typedef int (*SSL_write_t)(SSL*, const void*, int);
typedef SSL* (*SSL_new_t)(SSL_CTX*);
typedef const SSL_METHOD* (*SSLv23_client_method_t)(void);
typedef int (*SSL_shutdown_t)(SSL*);
typedef int (*SSL_set_fd_t)(SSL*, int);
typedef int (*SSL_pending_t)(const SSL*);
typedef void (*SSL_load_error_strings_t)(void);
typedef int (*SSL_library_init_t)(void);
typedef void (*SSL_CTX_free_t)(SSL_CTX*);
typedef SSL_CTX* (*SSL_CTX_new_t)(const SSL_METHOD*);
typedef long (*SSL_CTX_ctrl_t)(SSL_CTX*, int, long, void*);

int load_ssl();
int unload_ssl();

#ifdef EGG_SSL_EXT
extern SSL_CTX *ssl_ctx;
extern char *tls_rand_file;
#endif
extern int ssl_use;

#endif /* !_LIBSSL_H */
