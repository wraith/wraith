#ifndef _NET_H
#define _NET_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "types.h"
#include <netinet/in.h>
#include <sys/socket.h>


#ifdef HAVE_OPENSSL_SSL_H
# ifdef USE_SSL
#  include <openssl/ssl.h>
#  include <openssl/rand.h>
#  include <openssl/err.h>
#  undef HAVE_SSL
# endif /* USE_SSL */
/* #define HAVE_SSL 1 */
#endif /* HAVE_OPENSSL_SSL_H */

/*
 * Enable IPv6 debugging?
 */
#define DEBUG_IPV6 1
#define HAVE_IPV6 1

/* IPv6 sanity checks. */
#ifdef USE_IPV6
#  ifndef HAVE_IPV6
#    undef USE_IPV6
#  endif
#  ifndef HAVE_GETHOSTBYNAME2
#    ifndef HAVE_GETIPNODEBYNAME
#      undef USE_IPV6
#    endif
#  endif
#endif

#define SGRAB 2011         /* How much data to allow through sockets. */

enum {
  EGG_OPTION_SET        = 1,    /* Set option(s).               */
  EGG_OPTION_UNSET      = 2     /* Unset option(s).             */
};

/* Socket flags:
 */
#define SOCK_UNUSED     0x0001  /* empty socket                         */
#define SOCK_BINARY     0x0002  /* do not buffer input                  */
#define SOCK_LISTEN     0x0004  /* listening port                       */
#define SOCK_CONNECT    0x0008  /* connection attempt                   */
#define SOCK_NONSOCK    0x0010  /* used for file i/o on debug           */
#define SOCK_STRONGCONN 0x0020  /* don't report success until sure      */
#define SOCK_EOFD       0x0040  /* it EOF'd recently during a write     */
#define SOCK_PROXYWAIT  0x0080  /* waiting for SOCKS traversal          */
#define SOCK_PASS       0x0100  /* passed on; only notify in case
                                   of traffic                           */
#define SOCK_VIRTUAL    0x0200  /* not-connected socket (dont read it!) */
#define SOCK_BUFFER     0x0400  /* buffer data; don't notify dcc funcs  */

/* Flags to sock_has_data
 */
enum {
  SOCK_DATA_OUTGOING,           /* Data in out-queue?                   */
  SOCK_DATA_INCOMING            /* Data in in-queue?                    */
};


#define iptolong(a)             (0xffffffff &                           \
                                 (long) (htonl((unsigned long) a)))
#define CONNECT_SSL 1
#define ACCEPT_SSL 2

#ifdef USE_IPV6
#define SIZEOF_SOCKADDR(so) ((so).sa.sa_family == AF_INET6 ? sizeof(so.sin6) : sizeof(so.sin))
#else
#define SIZEOF_SOCKADDR(so) (sizeof(so.sin))
#endif /* USE_IPV6 */

#if !defined(IN6_IS_ADDR_V4MAPPED)
# define IN6_IS_ADDR_V4MAPPED(a) \
        ((((u_int32_t *) (a))[0] == 0) && (((u_int32_t *) (a))[1] == 0) && \
         (((u_int32_t *) (a))[2] == htonl (0xffff)))
#endif /* !defined(IN6_IS_ADDR_V4MAPPED) */

union sockaddr_union {
  struct sockaddr sa;
  struct sockaddr_in sin;
#ifdef USE_IPV6
  struct sockaddr_in6 sin6;
#endif /* USE_IPV6 */
};

/* This is used by the net module to keep track of sockets and what's
 * queued on them
 */
typedef struct {
  int            sock;
  short          flags;
  char          *inbuf;
  char          *outbuf;
  unsigned long  outbuflen;             /* Outbuf could be binary data  */
  int encstatus;                        /* encrypted botlink */
  int oseed;                            /* botlink out seed */
  int iseed;                            /* botlink in seed */
  char okey[33];                        /* botlink enckey: out */
  char ikey[33];                        /* botlink enckey: in  */
  int gz; /* gzip compression */
  unsigned long  inbuflen;              /* Inbuf could be binary data   */
#ifdef USE_IPV6
  unsigned int af;
#endif /* USE_IPV6 */
#ifdef HAVE_SSL
  SSL           *ssl;
#endif /* HAVE_SSL */
} sock_list;


#ifndef MAKING_MODS

# define killsock(x)     	real_killsock((x),__FILE__,__LINE__)

IP my_atoul(char *);
#ifdef HAVE_SSL
int ssl_cleanup();
#endif /* HAVE_SSL */
int ssl_link(int, int);
char *myipstr(int);
IP getmyip();
void cache_my_ip();
void neterror(char *);
void setsock(int, int);
int allocsock(int, int);
#ifdef USE_IPV6
int getsock(int, int);
#else
int getsock(int);
#endif /* USE_IPV6 */
int sockprotocol(int);
int hostprotocol(char *);
char *hostnamefromip(unsigned long);
void dropssl(int);
void real_killsock(int, const char *, int);
int answer(int, char *, unsigned long *, unsigned short *, int);
int findanyidx(register int);
inline int open_listen(int *);
inline int open_listen_by_af(int *, int);
#ifdef USE_IPV6
int open_address_listen(IP, int, int *);
#else
int open_address_listen(IP, int *);
#endif /* USE_IPV6 */
int open_telnet(char *, int);
int open_telnet_dcc(int, char *, char *);
int open_telnet_raw(int, char *, int);
void tputs(int, char *, unsigned int);
void dequeue_sockets();
int sockgets(char *, int *);
void tell_netdebug(int);
int sanitycheck_dcc(char *, char *, char *, char *);
int hostsanitycheck_dcc(char *, char *, IP, char *, char *);
char *iptostr(IP);
int sock_has_data(int, int);
int sockoptions(int sock, int operation, int sock_options);
int flush_inbuf(int idx);

#endif /* !MAKING_MODS */

#endif /* !_NET_H */
