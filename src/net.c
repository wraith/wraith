/* 
 * net.c -- handles:
 *   all raw network i/o
 * 
 */

#include <fcntl.h>
#include "main.h"
#include "proto.h"
#include <limits.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#if HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */
#include <netinet/in.h>
#include <arpa/inet.h>		/* is this really necessary? */
#include <errno.h>

#include <sys/stat.h>


#if HAVE_UNISTD_H
#  include <unistd.h>
#endif /* HAVE_UNITSTD_H */
#include <setjmp.h>

#if !HAVE_GETDTABLESIZE
#  ifdef FD_SETSIZE
#    define getdtablesize() FD_SETSIZE
#  else
#    define getdtablesize() 200
#  endif
#endif /* !HAVE_GETDTABLESIZE */

extern struct dcc_t	*dcc;
extern int		 backgrd, use_stderr, resolve_timeout, dcc_total;
extern unsigned long	 otraffic_irc_today, otraffic_bn_today,
			 otraffic_dcc_today, otraffic_filesys_today,
			 otraffic_trans_today, otraffic_unknown_today;


#ifdef HAVE_SSL
  SSL_CTX 		*ssl_c_ctx = NULL, *ssl_s_ctx = NULL;
  char 			*tls_rand_file = NULL;
#endif /* HAVE_SSL */

union sockaddr_union cached_myip4_so;
#ifdef USE_IPV6
union sockaddr_union cached_myip6_so;
unsigned long notalloc = 0;
#endif /* USE_IPV6 */

char	hostname[121] = "";	/* Hostname can be specified in the config
				   file					    */
char	myip[121] = "";		/* IP can be specified in the config file   */
char    myip6[121] = "";        /* IP can be specified in the config file   */
char    hostname6[121] = "";    /* Hostname can be specified in the config file */
char	firewall[121] = "";	/* Socks server for firewall		    */
int	firewallport = 1080;	/* Default port of Sock4/5 firewalls	    */
char	botuser[21] = "wraith"; /* Username of the user running the bot    */
int	dcc_sanitycheck = 0;	/* We should do some sanity checking on dcc
				   connections.				    */
sock_list *socklist = NULL;	/* Enough to be safe			    */
int	MAXSOCKS = 0;
jmp_buf	alarmret;		/* Env buffer for alarm() returns	    */

/* Types of proxy */
#define PROXY_SOCKS   1
#define PROXY_SUN     2


/* I need an UNSIGNED long for dcc type stuff
 */
IP my_atoul(char *s)
{
  IP ret = 0;

  while ((*s >= '0') && (*s <= '9')) {
    ret *= 10;
    ret += ((*s) - '0');
    s++;
  }
  return ret;
}

int hostprotocol(char *host)
{
#ifdef USE_IPV6
  struct hostent *he;
#  ifndef HAVE_GETHOSTBYNAME2
  int error_num;
#  endif /* !HAVE_GETHOSTBYNAME2 */

  if (!setjmp(alarmret)) {
    alarm(resolve_timeout);

#  ifdef HAVE_GETHOSTBYNAME2
    he = gethostbyname2(host, AF_INET6);
#  else
    he = getipnodebyname(host, AF_INET6, AI_DEFAULT, &error_num);
#  endif /* HAVE_GETHOSTBYNAME2 */
    alarm(0);
  } else
    he = NULL;
  if (!he)
    return AF_INET;
  return AF_INET6;
#else
  return 0;
#endif /* USE_IPV6 */
}

/* get the protocol used on a socket */
int sockprotocol(int socket)
{
  struct sockaddr sa;
  int i = sizeof(sa);

  if (getsockname(socket, &sa, &i))
    return -1;
  else
    return sa.sa_family;
}

/* AF_INET-independent resolving routine */
int get_ip(char *hostname, union sockaddr_union *so)
{
#ifdef USE_IPV6
  struct addrinfo hints, *ai, *res;
  int error;
#else
  struct hostent *hp;
#endif /* USE_IPV6 */

  egg_memset(so, 0, sizeof(union sockaddr_union));
  debug1(STR("get_ip(%s)"), hostname);

  if (!hostname || !hostname[0])
    return 1;
#ifdef USE_IPV6
  egg_memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = SOCK_STREAM;

  if ((error = getaddrinfo(hostname, NULL, &hints, &res)))
    return error;

  error = 1;
  for (ai = res; ai != NULL; ai = ai->ai_next) {
    if ((ai->ai_family == AF_INET6) || (ai->ai_family == AF_INET)) {
      memcpy(so, ai->ai_addr, sizeof(union sockaddr_union));
      error = 0;
      break;
    }
  }

  freeaddrinfo(res);
  return error;
#else

  if (!(hp = gethostbyname(hostname)))
    return -1;

  memcpy(&so->sin.sin_addr, hp->h_addr, 4);
  so->sin.sin_family = AF_INET;
  return 0;
#endif /* USE_IPV6 */
}

#ifdef HAVE_SSL
int seed_PRNG(void)
{
  char stackdata[1024];
  static char rand_file[300];
  FILE *fh = 0;
#if OPENSSL_VERSION_NUMBER >= 0x00905100
  if (RAND_status()) return 0;
#endif /* OPENSSL_VERSION_NUMBER */
  if ((fh = fopen("/dev/urandom", "r"))) {
    fclose(fh);
    return 0;
  }
  if (RAND_file_name(rand_file, sizeof(rand_file)))
    tls_rand_file = rand_file;
  else
    return 1;
  if (!RAND_load_file(rand_file, 1024)) {
    unsigned int c;
    c = time(NULL);
    RAND_seed(&c, sizeof(c));
    c = getpid();
    RAND_seed(&c, sizeof(c));
    RAND_seed(stackdata, sizeof(stackdata));
  }
#if OPENSSL_VERSION_NUMBER >= 0x00905100
  if (!RAND_status()) return 2;
#endif /* OPENSSL_VERSION_NUMBER >= 0x00905100 */
  return 0;
}
#endif /* HAVE_SSL */

/* Initialize the socklist
 */
void init_net()
{
  int i;

  for (i = 0; i < MAXSOCKS; i++) {
    egg_bzero(&socklist[i], sizeof(socklist[i]));
#ifdef HAVE_SSL
    socklist[i].ssl=NULL;
#endif /* HAVE_SSL */
    socklist[i].flags = SOCK_UNUSED;
  }
#ifdef HAVE_SSL
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
  ssl_c_ctx = SSL_CTX_new(SSLv23_client_method());
  ssl_s_ctx = SSL_CTX_new(SSLv23_server_method());
  if (!ssl_c_ctx || !ssl_s_ctx)
    fatal("SSL Inititlization failed", 0);
  if (seed_PRNG())
    fatal("SSL PRNG seeding failed!", 0);
#endif /* HAVE_SSL */
}

#ifdef HAVE_SSL
int ssl_cleanup() {
 if (ssl_c_ctx) {
    SSL_CTX_free(ssl_c_ctx);
    ssl_c_ctx = NULL;
  }
  if (ssl_s_ctx) {
    SSL_CTX_free(ssl_s_ctx);
    ssl_s_ctx = NULL;
  }
  if (tls_rand_file) RAND_write_file(tls_rand_file);
 return 0;
}
#endif /* HAVE_SSL */


int expmem_net()
{
  int i, tot = 0;

  for (i = 0; i < MAXSOCKS; i++) {
    if (!(socklist[i].flags & SOCK_UNUSED)) {
      if (socklist[i].inbuf != NULL)
	tot += strlen(socklist[i].inbuf) + 1;
      if (socklist[i].outbuf != NULL)
	tot += socklist[i].outbuflen;
    }
  }
  return tot;
}

/* Get my ipv? ip
 */
char *myipstr(int af_type)
{

#ifdef USE_IPV6
  if (af_type == 6) {
    static char s[UHOSTLEN + 1];
    egg_inet_ntop(AF_INET6, &cached_myip6_so.sin6.sin6_addr, s, 119);
    s[120] = 0;
    return s;
  } else
#endif /* USE_IPV6 */
    if (af_type == 4) {
      static char s[UHOSTLEN + 1];
      egg_inet_ntop(AF_INET, &cached_myip4_so.sin.sin_addr, s, 119);
      s[120] = 0;
      return s;
    }

  return "";
}

/* Get my ip number
 */
IP getmyip() {
  return (IP) cached_myip4_so.sin.sin_addr.s_addr;
}

/* see if it's necessary to set inaddr_any... because if we can't resolve, we die anyway */
void cache_my_ip()
{
  char s[121];
  int error;
#ifdef USE_IPV6
  int any = 0;
#endif /* USE_IPV6 */

  debug0(STR("cache_my_ip()"));
  egg_memset(&cached_myip4_so, 0, sizeof(union sockaddr_union));

#ifdef USE_IPV6
  egg_memset(&cached_myip6_so, 0, sizeof(union sockaddr_union));

  if (myip6 != NULL && myip6[1]) {
    sdprintf("myip6: %s", myip6);
    if (get_ip(myip6, &cached_myip6_so))
      any = 1;
  } else if (hostname != NULL && hostname6[1]) {
    sdprintf("myhostname6: %s", hostname6);
    if (get_ip(hostname6, &cached_myip6_so))
      any = 1;
  } else
    any = 1;

  if (any) {
    sdprintf("IPV6 addr_any is set.");
    cached_myip6_so.sin6.sin6_family = AF_INET6;
    cached_myip6_so.sin6.sin6_addr = in6addr_any;
  }
#endif /* USE_IPV6 */

  error = 0;
  if (myip[0]) {
    if (get_ip(myip, &cached_myip4_so))
      error = 1;
  } else if (hostname[0]) {
    if (get_ip(hostname, &cached_myip4_so))
      error = 2;
  } else {
    gethostname(s, 120);
    if (get_ip(hostname, &cached_myip4_so)) {
//      error = 3;
      cached_myip4_so.sin.sin_family = AF_INET;
      cached_myip4_so.sin.sin_addr.s_addr = INADDR_ANY;
    }
  }

  if (error) {
    putlog(LOG_DEBUG, "*", STR("Hostname self-lookup error: %d"), error);
    fatal(STR("Hostname self-lookup failed."), 0);
  }
}

void neterror(char *s)
{
  switch (errno) {
  case EADDRINUSE:
    strcpy(s, "Address already in use");
    break;
  case EADDRNOTAVAIL:
    strcpy(s, "Address invalid on remote machine");
    break;
  case EAFNOSUPPORT:
    strcpy(s, "Address family not supported");
    break;
  case EALREADY:
    strcpy(s, "Socket already in use");
    break;
  case EBADF:
    strcpy(s, "Socket descriptor is bad");
    break;
  case ECONNREFUSED:
    strcpy(s, "Connection refused");
    break;
  case EFAULT:
    strcpy(s, "Namespace segment violation");
    break;
  case EINPROGRESS:
    strcpy(s, "Operation in progress");
    break;
  case EINTR:
    strcpy(s, "Timeout");
    break;
  case EINVAL:
    strcpy(s, "Invalid namespace");
    break;
  case EISCONN:
    strcpy(s, "Socket already connected");
    break;
  case ENETUNREACH:
    strcpy(s, "Network unreachable");
    break;
  case ENOTSOCK:
    strcpy(s, "File descriptor, not a socket");
    break;
  case ETIMEDOUT:
    strcpy(s, "Connection timed out");
    break;
  case ENOTCONN:
    strcpy(s, "Socket is not connected");
    break;
  case EHOSTUNREACH:
    strcpy(s, "Host is unreachable");
    break;
  case EPIPE:
    strcpy(s, "Broken pipe");
    break;
#ifdef ECONNRESET
  case ECONNRESET:
    strcpy(s, "Connection reset by peer");
    break;
#endif /* ECONNRESET */
#ifdef EACCES
  case EACCES:
    strcpy(s, "Permission denied");
    break;
#endif /* EACCESS */
#ifdef EMFILE
  case EMFILE:
    strcpy(s, "Too many open files");
    break;
#endif /* EMFILE */
  case 0:
    strcpy(s, "Error 0");
    break;
  default:
    sprintf(s, "Unforseen error %d", errno);
    break;
  }
}

/* Sets/Unsets options for a specific socket.
 * 
 * Returns:  0   - on success
 *           -1  - socket not found
 *           -2  - illegal operation
 */
int sockoptions(int sock, int operation, int sock_options)
{
  int i;

  for (i = 0; i < MAXSOCKS; i++)
    if ((socklist[i].sock == sock) && !(socklist[i].flags & SOCK_UNUSED)) {
      if (operation == EGG_OPTION_SET)
	      socklist[i].flags |= sock_options;
      else if (operation == EGG_OPTION_UNSET)
	      socklist[i].flags &= ~sock_options;
      else
	      return -2;
      return 0;
    }
  return -1;
}

/* Return a free entry in the socket entry
 */
int allocsock(int sock, int options)
{
  int i;

  for (i = 0; i < MAXSOCKS; i++) {
    if (socklist[i].flags & SOCK_UNUSED) {
      /* yay!  there is table space */
      socklist[i].inbuf = socklist[i].outbuf = NULL;
      socklist[i].inbuflen = socklist[i].outbuflen = 0;
#ifdef HAVE_SSL
      socklist[i].ssl = NULL;
#endif /* HAVE_SSL */
      socklist[i].flags = options;
      socklist[i].sock = sock;
      socklist[i].encstatus = 0;
      socklist[i].gz = 0;
      egg_bzero(&socklist[i].okey, sizeof(socklist[i].okey));
      egg_bzero(&socklist[i].ikey, sizeof(socklist[i].ikey));
      return i;
    }
  }
  fatal("Socket table is full!", 0);
  return -1; /* Never reached */
}

/* Request a normal socket for i/o
 */
void setsock(int sock, int options)
{
  int i = allocsock(sock, options), parm;

  if (((sock != STDOUT) || backgrd) && !(socklist[i].flags & SOCK_NONSOCK)) {
    parm = 1;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *) &parm, sizeof(int));

    parm = 0;
    setsockopt(sock, SOL_SOCKET, SO_LINGER, (void *) &parm, sizeof(int));
  }
  if (options & SOCK_LISTEN) {
    /* Tris says this lets us grab the same port again next time */
    parm = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &parm, sizeof(int));
  }
  /* Yay async i/o ! */
  fcntl(sock, F_SETFL, O_NONBLOCK);

}

#ifdef USE_IPV6
int getsock(int options, int af_def)
{
#else
int getsock(int options)
{
  int af_def = AF_INET;
#endif /* USE_IPV6 */
  int sock;
  sock = socket(af_def, SOCK_STREAM, 0);

  if (sock >= 0)
    setsock(sock, options);
  else
    putlog(LOG_MISC, "*", "Warning: Can't create new socket!");
  return sock;
}

void dropssl(register int sock)
{
#ifdef HAVE_SSL
  int i;

  if (sock < 0)
    return;
  for (i = 0; (i < MAXSOCKS); i++)
    if (socklist[i].sock == sock) break;

  if (socklist[i].ssl) {
    SSL_set_quiet_shutdown(socklist[i].ssl, 1);
    SSL_shutdown(socklist[i].ssl);
    usleep(1000 * 500);
    SSL_free(socklist[i].ssl);
    usleep(1000 * 500);
    socklist[i].ssl = NULL;
  }
#endif /* HAVE_SSL */
}

/* Done with a socket
 */
void real_killsock(register int sock, const char *file, int line)
{
  register int	i;

  /* Ignore invalid sockets.  */
  if (sock < 0)
    return;
  for (i = 0; i < MAXSOCKS; i++) {
    if ((socklist[i].sock == sock) && !(socklist[i].flags & SOCK_UNUSED)) {
      dropssl(sock);
      close(socklist[i].sock);
      if (socklist[i].inbuf != NULL) {
	nfree(socklist[i].inbuf);
	socklist[i].inbuf = NULL;
      }
      if (socklist[i].outbuf != NULL) {
	nfree(socklist[i].outbuf);
	socklist[i].outbuf = NULL;
	socklist[i].outbuflen = 0;
      }
      egg_bzero(&socklist[i],sizeof(socklist[i]));
      socklist[i].flags = SOCK_UNUSED;
      return;
    }
  }
  putlog(LOG_MISC, "*", "Attempt to kill un-allocated socket %d %s:%d !!", sock, file, line);
}

/* Send connection request to proxy
 */
static int proxy_connect(int sock, char *host, int port, int proxy)
{
#ifdef USE_IPV6
  unsigned char x[32];
  int af_ty;
#else
  unsigned char x[10];
#endif /* USE_IPV6 */
  struct hostent *hp;
  char s[256];
  int i;

#ifdef USE_IPV6
  af_ty = sockprotocol(sock);
#endif /* USE_IPV6 */
  /* socks proxy */
  if (proxy == PROXY_SOCKS) {
    /* numeric IP? */
#ifdef USE_IPV6
    if ((host[strlen(host) - 1] >= '0' && host[strlen(host) - 1] <= '9') && af_ty != AF_INET6) {
#else
    if (host[strlen(host) - 1] >= '0' && host[strlen(host) - 1] <= '9') {
#endif /* USE_IPV6 */
      IP ip = ((IP) inet_addr(host));
      egg_memcpy(x, &ip, 4);
    } else {
      /* no, must be host.domain */
      if (!setjmp(alarmret)) {
#ifdef USE_IPV6
        alarm(resolve_timeout);
        if (af_ty == AF_INET6)
          hp = gethostbyname(host);
        else
#endif /* USE_IPV6 */
          hp = gethostbyname(host);
#ifdef USE_IPV6
        alarm(0);
#endif /* USE_IPV6 */
      } else
        hp = NULL;
      if (hp == NULL) {
        killsock(sock);
        return -2;
      }
      egg_memcpy(x, hp->h_addr, hp->h_length);
    }
    for (i = 0; i < MAXSOCKS; i++)
      if (!(socklist[i].flags & SOCK_UNUSED) && socklist[i].sock == sock)
        socklist[i].flags |= SOCK_PROXYWAIT;    /* drummer */
#ifdef USE_IPV6
    if (af_ty == AF_INET6)
      egg_snprintf(s, sizeof s,
                   "\004\001%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%s",
                   (port >> 8) % 256, (port % 256), x[0], x[1], x[2], x[3],
                   x[4], x[5], x[6], x[7], x[9], x[9], x[10], x[11],  x[12],
                   x[13], x[14], x[15], botuser);
    else
#endif /* USE_IPV6 */
      egg_snprintf(s, sizeof s, "\004\001%c%c%c%c%c%c%s", (port >> 8) % 256,
                   (port % 256), x[0], x[1], x[2], x[3], botuser);
    tputs(sock, s, strlen(botuser) + 9);        /* drummer */
  } else if (proxy == PROXY_SUN) {
    egg_snprintf(s, sizeof s, "%s %d\n", host, port);
    tputs(sock, s, strlen(s));  /* drummer */
  }
  return sock;
}

/* Starts a connection attempt to a socket
 * 
 * If given a normal hostname, this will be resolved to the corresponding
 * IP address first. PLEASE try to use the non-blocking dns functions
 * instead and then call this function with the IP address to avoid blocking.
 * 
 * returns <0 if connection refused:
 *   -1  neterror() type error
 *   -2  can't resolve hostname
 */
int open_telnet_raw(int sock, char *server, int sport)
{
  union sockaddr_union so;
  char host[121];
  int i, error, port, rc;
  volatile int proxy;

  /* firewall?  use socks */
  if (firewall[0]) {
    if (firewall[0] == '!') {
      proxy = PROXY_SUN;
      strcpy(host, &firewall[1]);
    } else {
      proxy = PROXY_SOCKS;
      strcpy(host, firewall);
    }
    port = firewallport;
  } else {
    proxy = 0;
    strcpy(host, server);
    port = sport;
  }

  error = 0;
  if (!setjmp(alarmret)) {
    alarm(resolve_timeout);
    if (!get_ip(host,&so)) {
      alarm(0);
      /* ok, we resolved it, bind an appropriate ip */
#ifdef USE_IPV6
      if (so.sa.sa_family == AF_INET6) {
        if (bind(sock, &cached_myip6_so.sa, SIZEOF_SOCKADDR(cached_myip6_so)) < 0) {
          killsock(sock);
          return -1;
        }
      } else {
#endif /* USE_IPV6 */
        if (bind(sock, &cached_myip4_so.sa, SIZEOF_SOCKADDR(cached_myip4_so)) < 0) {
          killsock(sock);
          return -3;
        }
#ifdef USE_IPV6
      }

      if (so.sa.sa_family == AF_INET6)
        so.sin6.sin6_port = htons(port);
      else
#endif /* USE_IPV6 */
        so.sin.sin_port = htons(port);

    } else {
      alarm(0);
      error = 1;
    }
  }

  /* I guess we broke something */
  if (error) {
    killsock(sock);
    return -2;
  }

  for (i = 0; i < MAXSOCKS; i++) {
    if (!(socklist[i].flags & SOCK_UNUSED) && (socklist[i].sock == sock))
      socklist[i].flags = (socklist[i].flags & ~SOCK_VIRTUAL) | SOCK_CONNECT;
  }

  rc = connect(sock, &so.sa, SIZEOF_SOCKADDR(so));
  if (rc < 0) {    if (errno == EINPROGRESS) {
      /* Firewall?  announce connect attempt to proxy */
      if (firewall[0])
	return proxy_connect(sock, server, sport, proxy);
      return sock;		/* async success! */
    } else
      return -1;
  }
  /* Synchronous? :/ */
  if (firewall[0])
    return proxy_connect(sock, server, sport, proxy);

  return sock;
}

/* Ordinary non-binary connection attempt */
int open_telnet(char *server, int port)
{
#ifdef USE_IPV6
  int sock = getsock(0, hostprotocol(server)) , ret = open_telnet_raw(sock, server, port);
#else
  int sock = getsock(0) , ret = open_telnet_raw(sock, server, port);
#endif /* USE_IPV6 */

  if (ret < 0)
    killsock(sock);
  return ret;
}

/* Returns a socket number for a listening socket that will accept any
 * connection on a certain address -- port # is returned in port
 *
 * 'addr' is ignored if af_def is AF_INET6 -poptix (02/03/03)
 */
#ifdef USE_IPV6
int open_address_listen(IP addr, int af_def, int *port)
#else
int open_address_listen(IP addr, int *port)
#endif /* USE_IPV6 */
 {
  int sock = 0;
  unsigned int addrlen;
  struct sockaddr_in name;

  if (firewall[0]) {
    /* FIXME: can't do listen port thru firewall yet */
    putlog(LOG_MISC, "*", "!! Cant open a listen port (you are using a "
           "firewall)");
    return -1;
  }
#ifdef USE_IPV6
  if (af_def == AF_INET6) {
    struct sockaddr_in6 name6;
    sock = getsock(SOCK_LISTEN, af_def);

    if (sock < 1)
    return -1;

    debug2(STR("Opening listen socket on port %d with AF_INET6, sock: %d"), *port, sock);
    egg_bzero((char *) &name6, sizeof(name6));
    name6.sin6_family = af_def;
    name6.sin6_port = htons(*port); /* 0 = just assign us a port */
    /* memcpy(&name6.sin6_addr, &in6addr_any, 16); */ /* this is the only way to get ipv6+ipv4 in 1 socket */
    memcpy(&name6.sin6_addr, &cached_myip6_so.sin6.sin6_addr, 16);
    if (bind(sock, (struct sockaddr *) &name6, sizeof(name6)) < 0) {
      killsock(sock);
      return -1;
    }
    addrlen = sizeof(name6);
    if (getsockname(sock, (struct sockaddr *) &name6, &addrlen) < 0) {
      killsock(sock);
      return -1;
    }
    *port = ntohs(name6.sin6_port);
    if (listen(sock, 1) < 0) {
      killsock(sock);
      return -1;
    }
  } else {
      sock = getsock(SOCK_LISTEN, AF_INET);
#else
      sock = getsock(SOCK_LISTEN);
#endif /* USE_IPV6 */

    if (sock < 1)
      return -1;

    debug2(STR("Opening listen socket on port %d with AF_INET, sock: %d"), *port, sock);
    egg_bzero((char *) &name, sizeof(struct sockaddr_in));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port); /* 0 = just assign us a port */
    name.sin_addr.s_addr = addr;
    if (bind(sock, (struct sockaddr *) &name, sizeof(name)) < 0) {
      killsock(sock);
      return -1;
    }
    /* what port are we on? */
    addrlen = sizeof(name);
    if (getsockname(sock, (struct sockaddr *) &name, &addrlen) < 0) {
      killsock(sock);
      return -1;
    }
    *port = ntohs(name.sin_port);
    if (listen(sock, 1) < 0) {
      killsock(sock);
      return -1;
    }
#ifdef USE_IPV6
  }
#endif /* USE_IPV6 */
  return sock;
}

/* Returns a socket number for a listening socket that will accept any
 * connection -- port # is returned in port
 */
inline int open_listen(int *port)
{
#ifdef USE_IPV6
  return open_address_listen(myip[0] ? getmyip() : INADDR_ANY, AF_INET, port);
#else
  return open_address_listen(myip[0] ? getmyip() : INADDR_ANY, port);
#endif /* USE_IPV6 */
}

/* Same as above, except this one can be called with an AF_ type
 * the above is being left in for compatibility, and should NOT LONGER BE USED IN THE CORE CODE.
 */

inline int open_listen_by_af(int *port, int af_def)
{
#ifdef USE_IPV6
  return open_address_listen(myip[0] ? getmyip() : INADDR_ANY, af_def, port);
#else
  return 0;
#endif /* USE_IPV6 */
}

int ssl_link(register int sock, int state)
{
#ifdef HAVE_SSL
  int err = 0, i = 0, errs = 0;

  debug2(STR("ssl_link(%d, %d)"), sock, state);
  for (i = 0; (i < MAXSOCKS); i++) {		
    if (socklist[i].sock == sock) break;
  }
  if (socklist[i].ssl) {
    putlog(LOG_ERROR, "*", "Switching to SSL (%d,%d) - already active", state, sock);
    return 0;
  }
  if (state == CONNECT_SSL) {
    socklist[i].ssl = SSL_new(ssl_c_ctx);
  } else if (state == ACCEPT_SSL) {
    socklist[i].ssl = SSL_new(ssl_s_ctx);
  }
  if (!socklist[i].ssl) {
    putlog(LOG_ERROR, "*", "Switching to SSL (%d) - SSL_new(%d) failed", sock, state);
    return 0;
  }
  if (!SSL_set_fd(socklist[i].ssl, socklist[i].sock)) {
    putlog(LOG_ERROR, "*", "SSL_set_fd(%d) (%d) failed", state, socklist[i].sock);
    return 0;
  }
  if (state == CONNECT_SSL) {
    SSL_set_connect_state(socklist[i].ssl);
  } else if (state == ACCEPT_SSL) {
    SSL_set_accept_state(socklist[i].ssl);
  } else {
    putlog(LOG_DEBUG, "*" STR("ssl_link(%d, 0?) NO STATE?"), sock);
    return 0;
  }

  if (state == CONNECT_SSL) {
    err = SSL_connect(socklist[i].ssl);
  } else if (state == ACCEPT_SSL) {
    err = SSL_accept(socklist[i].ssl);
  }
  if (!setjmp(alarmret)) {
      alarm(5); /* this is plenty of time */
      while ((err < 1) && (errno == EAGAIN)) {
        if (state == CONNECT_SSL) {
          err = SSL_connect(socklist[i].ssl);
        } else if (state == ACCEPT_SSL) {
          err = SSL_accept(socklist[i].ssl);
        }
//        if ((errs!=SSL_ERROR_WANT_READ)&&(errs!=SSL_ERROR_WANT_WRITE)&& (errs!=SSL_ERROR_WANT_X509_LOOKUP))
//          break; /* anything not one of these is a sufficient condition to break out... */
      }
      alarm(0);
  }
      errs = SSL_get_error(socklist[i].ssl, err);
        putlog(LOG_DEBUG, "*", STR("SSL_link(%d, %d) = %d, errs: %d (%d), %s"), sock, state, err, errs, errno, (char *)ERR_error_string(ERR_get_error(), NULL));
        if (errno) putlog(LOG_DEBUG, "*", STR("errno %d: %s"), errno, strerror(errno));
  if (err == 1) {
    putlog(LOG_ERROR, "*", "SSL_link(%d, %d) was successfull", sock, state);
    return 1;
  } else {
    putlog(LOG_ERROR, "*", "SSL_link(%d, %d) failed", sock, state);
    dropssl(socklist[i].sock);
  }
#endif /* HAVE_SSL */
  return 0;
}


/* Given a network-style IP address, returns the hostname. The hostname
 * will be in the "##.##.##.##" format if there was an error.
 * 
 * NOTE: This function is depreciated. Try using the async dns approach
 *       instead.
 */
char *hostnamefromip(unsigned long ip)
{
  struct hostent *hp;
  unsigned long addr = ip;
  unsigned char *p;
  static char s[UHOSTLEN];

  if (!setjmp(alarmret)) {
    alarm(resolve_timeout);
    hp = gethostbyaddr((char *) &addr, sizeof(addr), AF_INET);
    alarm(0);
  } else {
    hp = NULL;
  }
  if (hp == NULL) {
    p = (unsigned char *) &addr;
    sprintf(s, "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
    return s;
  }
  strncpyz(s, hp->h_name, sizeof s);
  return s;
}

/* Returns the given network byte order IP address in the
 * dotted format - "##.##.##.##"
 */
char *iptostr(IP ip)
{
  struct in_addr a;

  a.s_addr = ip;
  return inet_ntoa(a);
}

/* Short routine to answer a connect received on a socket made previously
 * by open_listen ... returns hostname of the caller & the new socket
 * does NOT dispose of old "public" socket!
 */
int answer(int sock, char *caller, unsigned long *ip, unsigned short *port,
	   int binary)
{
  int new_sock;
  unsigned int addrlen;
  struct sockaddr_in from;
#ifdef USE_IPV6
  int af_ty = sockprotocol(sock);
  struct sockaddr_in6 from6;

  if (af_ty == AF_INET6) {
    addrlen = sizeof(from6);
    new_sock = accept(sock, (struct sockaddr *) &from6, &addrlen);
  } else {
#endif /* USE_IPV6 */
    addrlen = sizeof(struct sockaddr);
    new_sock = accept(sock, (struct sockaddr *) &from, &addrlen);
#ifdef USE_IPV6
  }
#endif /* USE_IPV6 */
  
  if (new_sock < 0)
    return -1;
  if (ip != NULL) {
#ifdef USE_IPV6
    /* Detect IPv4 in IPv6 mapped address .... */
    if (af_ty == AF_INET6 && (!IN6_IS_ADDR_V4MAPPED(&from6.sin6_addr))) {
      egg_inet_ntop(AF_INET6, &from6.sin6_addr, caller, 119);
      caller[120] = 0;
      *ip = notalloc;
    } else if (IN6_IS_ADDR_V4MAPPED(&from6.sin6_addr)) {    /* ...and convert it to plain (AF_INET) IPv4 address (openssh) */
      struct sockaddr_in *from4 = (struct sockaddr_in *)&from6;
      struct in_addr addr;

      memcpy(&addr, ((char *)&from6.sin6_addr) + 12, sizeof(addr));
      egg_memset(&from, 0, sizeof(from));

      from4->sin_family = AF_INET;
      addrlen = sizeof(*from4);
      memcpy(&from4->sin_addr, &addr, sizeof(addr));

      *ip = from4->sin_addr.s_addr;
      strncpyz(caller, iptostr(*ip), 121);
      *ip = ntohl(*ip);
    } else {
#endif /* USE_IPV6 */
      *ip = from.sin_addr.s_addr;
      /* This is now done asynchronously. We now only provide the IP address.
       *
       * strncpy(caller, hostnamefromip(*ip), 120);
       */
      strncpyz(caller, iptostr(*ip), 121);
      *ip = ntohl(*ip);
#ifdef USE_IPV6 
    }
#endif /* USE_IPV6 */
  }
  if (port != NULL) {
#ifdef USE_IPV6
      if (af_ty == AF_INET6)
        *port = ntohs(from6.sin6_port);
      else
#endif /* USE_IPV6 */
        *port = ntohs(from.sin_port);
    }
  /* Set up all the normal socket crap */
  setsock(new_sock, (binary ? SOCK_BINARY : 0));
  return new_sock;
}

/* Like open_telnet, but uses server & port specifications of dcc
 */
int open_telnet_dcc(int sock, char *server, char *port)
{
  int p;
  unsigned long addr;
  char sv[500];
  unsigned char c[4];

#ifdef DEBUG_IPV6
  debug1(STR("open_telnet_dcc %s"), server);
#endif /* DEBUG_IPV6 */
  if (port != NULL)
    p = atoi(port);
  else
    p = 2000;
#ifdef USE_IPV6
  if (sockprotocol(sock) == AF_INET6) {
#  ifdef DEBUG_IPV6
    debug0(STR("open_telnet_dcc, af_inet6!"));
#  endif /* DEBUG_IPV6 */
    strncpyz(sv, server, sizeof sv);
    debug2(STR("%s should be %s"),sv,server);
  } else {
#endif /* USE_IPV6 */
    if (server != NULL)
      addr = my_atoul(server);
    else
      addr = 0L;
    if (addr < (1 << 24))
      return -3;			/* fake address */
    c[0] = (addr >> 24) & 0xff;
    c[1] = (addr >> 16) & 0xff;
    c[2] = (addr >> 8) & 0xff;
    c[3] = addr & 0xff;
    sprintf(sv, "%u.%u.%u.%u", c[0], c[1], c[2], c[3]);
#ifdef USE_IPV6
    }
  /* strcpy(sv,hostnamefromip(addr)); */
#  ifdef DEBUG_IPV6
  debug3(STR("open_telnet_raw %s %d %d"), sv, sock,p);
#  endif /* DEBUG_IPV6 */
#endif /* USE_IPV6 */
  p = open_telnet_raw(sock, sv, p);
  return p;
}

/* Attempts to read from all the sockets in socklist
 * fills s with up to 511 bytes if available, and returns the array index
 * 
 * 		on EOF:  returns -1, with socket in len
 *     on socket error:  returns -2
 * if nothing is ready:  returns -3
 */
static int sockread(char *s, int *len)
{
  fd_set fd;
  int fds, i, x, fdtmp;
  struct timeval t;
  int grab = SGRAB;

  fds = getdtablesize();
#ifdef FD_SETSIZE
  if (fds > FD_SETSIZE)
    fds = FD_SETSIZE;		/* Fixes YET ANOTHER freebsd bug!!! */
#endif /* FD_SETSIZE */
  /* timeout: 1 sec */
  t.tv_sec = 1;
  t.tv_usec = 0;
  FD_ZERO(&fd);
  
  for (i = 0; i < MAXSOCKS; i++)
    if (!(socklist[i].flags & (SOCK_UNUSED | SOCK_VIRTUAL))) {
      if ((socklist[i].sock == STDOUT) && !backgrd)
	fdtmp = STDIN;
      else
	fdtmp = socklist[i].sock;
      /* 
       * Looks like that having more than a call, in the same
       * program, to the FD_SET macro, triggers a bug in gcc.
       * SIGBUS crashing binaries used to be produced on a number
       * (prolly all?) of 64 bits architectures.
       * Make your best to avoid to make it happen again.
       * 
       * ITE
       */
      FD_SET(fdtmp, &fd);
    }
#ifdef HPUX_HACKS
#ifndef HPUX10_HACKS
  x = select(fds, (int *) &fd, (int *) NULL, (int *) NULL, &t);
#else /* !HPUX10_HACKS */
  x = select(fds, &fd, NULL, NULL, &t);
#endif /* HPUX10_HACKS */
#else /* !HPUX_HACKS */
  x = select(fds, &fd, NULL, NULL, &t);
#endif /* HPUX_HACKS */
  if (x > 0) {
    /* Something happened */
    for (i = 0; i < MAXSOCKS; i++) {
      if ((!(socklist[i].flags & SOCK_UNUSED)) && ((FD_ISSET(socklist[i].sock, &fd)) ||
#ifdef HAVE_SSL
          ((socklist[i].ssl) && (SSL_pending(socklist[i].ssl))) ||
#endif /* HAVE_SSL */
	  ((socklist[i].sock == STDOUT) && (!backgrd) && (FD_ISSET(STDIN, &fd))))) {
	if (socklist[i].flags & (SOCK_LISTEN | SOCK_CONNECT)) {
	  /* Listening socket -- don't read, just return activity */
	  /* Same for connection attempt */
	  /* (for strong connections, require a read to succeed first) */
	  if (socklist[i].flags & SOCK_PROXYWAIT) { /* drummer */
	    /* Hang around to get the return code from proxy */
	    grab = 10;
	  } else if (!(socklist[i].flags & SOCK_STRONGCONN)) {
	    debug1(STR("net: connect! sock %d"), socklist[i].sock);
	    s[0] = 0;
	    *len = 0;
#ifdef HAVE_SSL
/*            debug0(STR("CALLING SSL_LINK() FROM SOCKREAD"));
//            if (!ssl_link(socklist[i].sock))
              debug0(STR("SSL_LINK FAILED"));
            debug0(STR("BACK FROM SSL_LINK()")); */
#endif /* HAVE_SSL */
	    return i;
	  }
	} else if (socklist[i].flags & SOCK_PASS) {
	  s[0] = 0;
	  *len = 0;
	  return i;
	}
	errno = 0;
	if ((socklist[i].sock == STDOUT) && !backgrd)
	  x = read(STDIN, s, grab);
	else {
#ifdef HAVE_SSL
          if (socklist[i].ssl) {
            x = SSL_read(socklist[i].ssl, s, grab);
            if (x < 0) {
              int err = SSL_get_error(socklist[i].ssl, x);
              x = -1;
              switch (err) {
                case SSL_ERROR_WANT_READ:
                  errno = EAGAIN;
                  break;
                case SSL_ERROR_WANT_WRITE:
                  errno = EAGAIN;
                  break;
                case SSL_ERROR_WANT_X509_LOOKUP:
                  errno = EAGAIN;
                  break;
              }
            }
          } else 
#endif /* HAVE_SSL */
            x = read(socklist[i].sock, s, grab);
        }
	if (x <= 0) {		/* eof */
	  if (errno != EAGAIN) { /* EAGAIN happens when the operation would block 
				    on a non-blocking socket, if the socket is going
				    to die, it will die later, otherwise it will connect */
	    *len = socklist[i].sock;
	    socklist[i].flags &= ~SOCK_CONNECT;
	    debug1(STR("net: eof!(read) socket %d"), socklist[i].sock);
	    return -1;
	  } else {
	    debug3(STR("sockread EAGAIN: %d %d (%s)"), socklist[i].sock, errno, strerror(errno));
	    continue; /* EAGAIN */
	  }
	}
	s[x] = 0;
	*len = x;
	if (socklist[i].flags & SOCK_PROXYWAIT) {
	  debug2(STR("net: socket: %d proxy errno: %d"), socklist[i].sock, s[1]);
	  socklist[i].flags &= ~(SOCK_CONNECT | SOCK_PROXYWAIT);
	  switch (s[1]) {
	  case 90:		/* Success */
	    s[0] = 0;
	    *len = 0;
	    return i;
	  case 91:		/* Failed */
	    errno = ECONNREFUSED;
	    break;
	  case 92:		/* No identd */
	  case 93:		/* Identd said wrong username */
	    /* A better error message would be "socks misconfigured"
	     * or "identd not working" but this is simplest.
	     */
	    errno = ENETUNREACH;
	    break;
	  }
	  *len = socklist[i].sock;
	  return -1;
	}
	return i;
      }
    }
  } else if (x == -1)
    return -2;			/* socket error */
  else {
    s[0] = 0;
    *len = 0;
  }
  return -3;
}

int prand(int *seed, int range)
{
  long long i1;

  i1 = *seed;
  i1 = (i1 * 0x08088405 + 1) & 0xFFFFFFFF;
  *seed = i1;
  i1 = (i1 * range) >> 32;
  return i1;
}

char *botlink_decrypt(int snum, char *src)
{
  char *line = NULL;
  int i;

  line = decrypt_string(socklist[snum].ikey, src);
  if (socklist[snum].iseed) {
    for (i = 0; i <= 3; i++)
      *(dword *) & socklist[snum].ikey[i * 4] = prand(&socklist[snum].iseed, 0xFFFFFFFF);
    if (!socklist[snum].iseed)
      socklist[snum].iseed++;
  }
  strcpy(src, line);
  nfree(line);
  return src;
}

char *botlink_encrypt(int snum, char *src)
{
  char *srcbuf = NULL, *buf = NULL, *line = NULL, *eol = NULL, *eline = NULL;
  int bufpos = 0, i;
  
  srcbuf = nmalloc(strlen(src) + 10);
  strcpy(srcbuf, src);
  line = srcbuf;
  if (!line)
    return NULL;
  eol = strchr(line, '\n');
  i = 0;
  while (eol) {
    *eol++ = 0;
    eline = encrypt_string(socklist[snum].okey, line);
    if (socklist[snum].oseed) {
      for (i = 0; i <= 3; i++)
        *(dword *) & socklist[snum].okey[i * 4] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      if (!socklist[snum].oseed)
        socklist[snum].oseed++;
    }
    buf = nrealloc(buf, bufpos + strlen(eline) + 1 + 9);
    strcpy((char *) &buf[bufpos], eline);
    strcat(buf, "\n");
    bufpos = strlen(buf);
    line = eol;
    eol = strchr(line, '\n');
    nfree(eline);
  }
  if (line[0]) {
    eline = encrypt_string(socklist[snum].okey, line);
    if (socklist[snum].oseed) {
       *(dword *) & socklist[snum].okey[i * 4] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      if (!socklist[snum].oseed)
       socklist[snum].oseed++;
    }
    buf = nrealloc(buf, bufpos + strlen(eline) + 1 + 9);
    strcpy((char *) &buf[bufpos], eline);
    strcat(buf, "\n");
    nfree(eline);
  }
  nfree(srcbuf);
  return buf;
}

/* sockgets: buffer and read from sockets
 * 
 * Attempts to read from all registered sockets for up to one second.  if
 * after one second, no complete data has been received from any of the
 * sockets, 's' will be empty, 'len' will be 0, and sockgets will return -3.
 * if there is returnable data received from a socket, the data will be
 * in 's' (null-terminated if non-binary), the length will be returned
 * in len, and the socket number will be returned.
 * normal sockets have their input buffered, and each call to sockgets
 * will return one line terminated with a '\n'.  binary sockets are not
 * buffered and return whatever coems in as soon as it arrives.
 * listening sockets will return an empty string when a connection comes in.
 * connecting sockets will return an empty string on a successful connect,
 * or EOF on a failed connect.
 * if an EOF is detected from any of the sockets, that socket number will be
 * put in len, and -1 will be returned.
 * the maximum length of the string returned is 512 (including null)
 *
 * Returns -4 if we handled something that shouldn't be handled by the
 * dcc functions. Simply ignore it.
 */

int sockgets(char *s, int *len)
{
  char xx[SGRAB+3], *p, *px;
  int ret, i, data = 0, grab = SGRAB+1;

  for (i = 0; i < MAXSOCKS; i++) {
    /* Check for stored-up data waiting to be processed */
    if (!(socklist[i].flags & SOCK_UNUSED) &&
	!(socklist[i].flags & SOCK_BUFFER) && (socklist[i].inbuf != NULL)) {
      if (!(socklist[i].flags & SOCK_BINARY)) {
	/* look for \r too cos windows can't follow RFCs */
	p = strchr(socklist[i].inbuf, '\n');
	if (p == NULL)
	  p = strchr(socklist[i].inbuf, '\r');
	if (p != NULL) {
	  *p = 0;
	  if (strlen(socklist[i].inbuf) > (grab - 2))
	    socklist[i].inbuf[(grab - 2)] = 0;
	  strcpy(s, socklist[i].inbuf);
	  px = (char *) nmalloc(strlen(p + 1) + 1);
	  strcpy(px, p + 1);
	  nfree(socklist[i].inbuf);
	  if (px[0])
	    socklist[i].inbuf = px;
	  else {
	    nfree(px);
	    socklist[i].inbuf = NULL;
	  }
	  /* Strip CR if this was CR/LF combo */
	  if (s[strlen(s) - 1] == '\r')
	    s[strlen(s) - 1] = 0;
          if (socklist[i].encstatus)
            botlink_decrypt(i, s);
	  *len = strlen(s);
	  return socklist[i].sock;
	}
      } else {
	/* Handling buffered binary data (must have been SOCK_BUFFER before). */
	if (socklist[i].inbuflen <= (grab - 2)) {
	  *len = socklist[i].inbuflen;
	  egg_memcpy(s, socklist[i].inbuf, socklist[i].inbuflen);
	  nfree(socklist[i].inbuf);
          socklist[i].inbuf = NULL;
	  socklist[i].inbuflen = 0;
	} else {
	  /* Split up into chunks of grab bytes. */
	  *len = grab - 2;
	  egg_memcpy(s, socklist[i].inbuf, *len);
	  egg_memcpy(socklist[i].inbuf, socklist[i].inbuf + *len, *len);
	  socklist[i].inbuflen -= *len;
	  socklist[i].inbuf = nrealloc(socklist[i].inbuf,
				       socklist[i].inbuflen);
	}
	return socklist[i].sock;
      }
    }
    /* Also check any sockets that might have EOF'd during write */
    if (!(socklist[i].flags & SOCK_UNUSED)
	&& (socklist[i].flags & SOCK_EOFD)) {
      s[0] = 0;
      *len = socklist[i].sock;
      return -1;
    }
  }
  /* No pent-up data of any worth -- down to business */
  *len = 0;
  ret = sockread(xx, len);
  if (ret < 0) {
    s[0] = 0;
    return ret;
  }
  /* Binary, listening and passed on sockets don't get buffered. */
  if (socklist[ret].flags & SOCK_CONNECT) {
    if (socklist[ret].flags & SOCK_STRONGCONN) {
      socklist[ret].flags &= ~SOCK_STRONGCONN;
      /* Buffer any data that came in, for future read. */
      socklist[ret].inbuflen = *len;
      socklist[ret].inbuf = (char *) nmalloc(*len + 1);
      /* It might be binary data. You never know. */
      egg_memcpy(socklist[ret].inbuf, xx, *len);
      socklist[ret].inbuf[*len] = 0;
    }
    socklist[ret].flags &= ~SOCK_CONNECT;
    s[0] = 0;
    return socklist[ret].sock;
  }
  if (socklist[ret].flags & SOCK_BINARY) {
    egg_memcpy(s, xx, *len);
    return socklist[ret].sock;
  }
  if ((socklist[ret].flags & SOCK_LISTEN) ||
      (socklist[ret].flags & SOCK_PASS))
    return socklist[ret].sock;
  if (socklist[ret].flags & SOCK_BUFFER) {
    socklist[ret].inbuf = (char *) nrealloc(socklist[ret].inbuf,
		    			    socklist[ret].inbuflen + *len + 1);
    egg_memcpy(socklist[ret].inbuf + socklist[ret].inbuflen, xx, *len);
    socklist[ret].inbuflen += *len;
    /* We don't know whether it's binary data. Make sure normal strings
       will be handled properly later on too. */
    socklist[ret].inbuf[socklist[ret].inbuflen] = 0;
    return -4;	/* Ignore this one. */
  }
  /* Might be necessary to prepend stored-up data! */
  if (socklist[ret].inbuf != NULL) {
    p = socklist[ret].inbuf;
    socklist[ret].inbuf = (char *) nmalloc(strlen(p) + strlen(xx) + 1);
    strcpy(socklist[ret].inbuf, p);
    strcat(socklist[ret].inbuf, xx);
    nfree(p);
    if (strlen(socklist[ret].inbuf) < grab) {
      strcpy(xx, socklist[ret].inbuf);
      nfree(socklist[ret].inbuf);
      socklist[ret].inbuf = NULL;
      socklist[ret].inbuflen = 0;
    } else {
      p = socklist[ret].inbuf;
    socklist[ret].inbuflen = strlen(p) - (grab - 2);
      socklist[ret].inbuf = (char *) nmalloc(socklist[ret].inbuflen + 1); 
//      socklist[ret].inbuf = (char *) nmalloc(strlen(p) - (grab - 3));
      strcpy(socklist[ret].inbuf, p + (grab - 2));
      *(p + (grab - 2)) = 0;
      strcpy(xx, p);
      nfree(p);
      /* (leave the rest to be post-pended later) */
    }
  }
  /* Look for EOL marker; if it's there, i have something to show */
  p = strchr(xx, '\n');
  if (p == NULL)
    p = strchr(xx, '\r');
  if (p != NULL) {
    *p = 0;
    strcpy(s, xx);
    strcpy(xx, p + 1);
    if (s[strlen(s) - 1] == '\r')
      s[strlen(s) - 1] = 0;
    data = 1;			/* DCC_CHAT may now need to process a
				   blank line */

/* NO! */

/* if (!s[0]) strcpy(s," ");  */
  } else {
    s[0] = 0;
    if (strlen(xx) >= (grab - 2)) {
      /* String is too long, so just insert fake \n */
      strcpy(s, xx);
      xx[0] = 0;
      data = 1;
    }
  }
  if (socklist[ret].encstatus)
    botlink_decrypt(ret, s);
  *len = strlen(s);
  /* Anything left that needs to be saved? */
  if (!xx[0]) {
    if (data)
      return socklist[ret].sock;
    else
      return -3;
  }
  /* Prepend old data back */
  if (socklist[ret].inbuf != NULL) {
    p = socklist[ret].inbuf;
    socklist[ret].inbuflen = strlen(p) + strlen(xx);
    socklist[ret].inbuf = (char *) nmalloc(socklist[ret].inbuflen + 1);
    strcpy(socklist[ret].inbuf, xx);
    strcat(socklist[ret].inbuf, p);
    nfree(p);
  } else {
    socklist[ret].inbuflen = strlen(xx);
    socklist[ret].inbuf = (char *) nmalloc(socklist[ret].inbuflen + 1);
    strcpy(socklist[ret].inbuf, xx);
  }
  if (data) {
    return socklist[ret].sock;
  } else {
    return -3;
  }
}
/* Dump something to a socket
 * 
 * NOTE: Do NOT put Contexts in here if you want DEBUG to be meaningful!!
 */
void tputs(register int z, char *s, unsigned int len)
{
  register int i, x, idx;
  char *p;
  static int inhere = 0;
  if (z < 0)
    return;			/* um... HELLO?!  sanity check please! */
  if (((z == STDOUT) || (z == STDERR)) && (!backgrd || use_stderr)) {
#ifdef EUSE_COLORPUTS
     colorputs(s);
     x = len;
#else /* !EUSE_COLORPUTS */
    write(z, s, len);
#endif /* EUSE_COLORPUTS */
    return;
  }
  for (i = 0; i < MAXSOCKS; i++) {
    if (!(socklist[i].flags & SOCK_UNUSED) && (socklist[i].sock == z)) {
      for (idx = 0; idx < dcc_total; idx++) {
        if (dcc[idx].sock == z) {
          if (dcc[idx].type) {
            if (dcc[idx].type->name) {
              if (!strncmp(dcc[idx].type->name, "BOT", 3)) {
                otraffic_bn_today += len;
                break;
              } else if (!strcmp(dcc[idx].type->name, "SERVER")) {
                otraffic_irc_today += len;
                break;
              } else if (!strncmp(dcc[idx].type->name, "CHAT", 4)) {
                otraffic_dcc_today += len;
                break;
              } else if (!strncmp(dcc[idx].type->name, "FILES", 5)) {
                otraffic_filesys_today += len;
                break;
              } else if (!strcmp(dcc[idx].type->name, "SEND")) {
                otraffic_trans_today += len;
                break;
              } else if (!strncmp(dcc[idx].type->name, "GET", 3)) {
                otraffic_trans_today += len;
                break;
              } else {
                otraffic_unknown_today += len;
                break;
              }
            }
          }
        }
      }
      if (socklist[i].encstatus) {
        if ((!s) || (!s[0])) {
         s = botlink_encrypt(i, s);
         len = strlen(s);
       }
       s = botlink_encrypt(i, s);
       len = strlen(s);
      }
      
      if (socklist[i].outbuf != NULL) {
	/* Already queueing: just add it */
	p = (char *) nrealloc(socklist[i].outbuf, socklist[i].outbuflen + len);
	egg_memcpy(p + socklist[i].outbuflen, s, len);
	socklist[i].outbuf = p;
	socklist[i].outbuflen += len;
        if (socklist[i].encstatus)
         nfree(s);
	return;
      }
      /* Try. */
#ifdef HAVE_SSL
      if (socklist[i].ssl) {
        x = SSL_write(socklist[i].ssl, s, len);
        if (x < 0) {
          int err = SSL_get_error(socklist[i].ssl, x);
          x = -1;
          switch (err) {
            case SSL_ERROR_WANT_READ:
              errno = EAGAIN;
              break;
            case SSL_ERROR_WANT_WRITE:
              errno = EAGAIN;
              break;
            case SSL_ERROR_WANT_X509_LOOKUP:
              errno = EAGAIN;
              break;
          }
        }
      } else
#endif /* HAVE_SSL */
//#ifdef HAVE_ZLIB_H
//      if (socklist[i].gz) { 		/* gzipped links */
//        FILE *fp;
//        fp = gzdopen(z, "wb0");
//        x = gzwrite(fp, s, len);
//        
//      } else
//#endif /* HAVE_ZLIB_H */
        x = write(z, s, len);
      if (x == (-1))
	x = 0;
      if (x < len) {
	/* Socket is full, queue it */
	socklist[i].outbuf = (char *) nmalloc(len - x);
	egg_memcpy(socklist[i].outbuf, &s[x], len - x);
	socklist[i].outbuflen = len - x;
      }
      if (socklist[i].encstatus)
       nfree(s);
      return;
    }
  }
  /* Make sure we don't cause a crash by looping here */
  if (!inhere) {
    inhere = 1;

    putlog(LOG_MISC, "*", "!!! writing to nonexistent socket: %d", z);
    s[strlen(s) - 1] = 0;
    putlog(LOG_MISC, "*", "!-> '%s'", s);

    inhere = 0;
  }

//  if (socklist[i].encstatus > 0)
//    nfree(s);
}

/* tputs might queue data for sockets, let's dump as much of it as
 * possible.
 */
void dequeue_sockets()
{
  int i, x;

  int z = 0, fds;
  fd_set wfds;
  struct timeval tv;

/* ^-- start poptix test code, this should avoid writes to sockets not ready to be written to. */
  fds = getdtablesize();

#ifdef FD_SETSIZE
  if (fds > FD_SETSIZE)
    fds = FD_SETSIZE;           /* Fixes YET ANOTHER freebsd bug!!! */
#endif /* FD_SETSIZE */
  FD_ZERO(&wfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0; 		/* we only want to see if it's ready for writing, no need to actually wait.. */
  for (i = 0; i < MAXSOCKS; i++) { 
    if (!(socklist[i].flags & SOCK_UNUSED) && socklist[i].outbuf != NULL) {
      FD_SET(socklist[i].sock, &wfds);
      z = 1; 
    }
  }
  if (!z)
    return; 			/* nothing to write */
#ifdef HPUX_HACKS
 #ifndef HPUX10_HACKS
  select(fds, (int *) NULL, (int *) &wfds, (int *) NULL, &tv);
 #else /* !HPUX10_HACKS */
  select(fds, NULL, &wfds, NULL, &tv);
 #endif /* HPUX10_HACKS */
#else /* !HPUX_HACKS */
  select(fds, NULL, &wfds, NULL, &tv);
#endif /* HPUX_HACKS */

/* end poptix */

  for (i = 0; i < MAXSOCKS; i++) { 
    if (!(socklist[i].flags & SOCK_UNUSED) &&
	(socklist[i].outbuf != NULL) && (FD_ISSET(socklist[i].sock, &wfds))) {
      /* Trick tputs into doing the work */
      errno = 0;
#ifdef HAVE_SSL
      if (socklist[i].ssl) {
        x = write(socklist[i].sock, socklist[i].outbuf, socklist[i].outbuflen);
        if (x < 0) {
          int err = SSL_get_error(socklist[i].ssl, x);
          x = -1;
          switch (err) {
            case SSL_ERROR_WANT_READ:
              errno = EAGAIN;
              break;
            case SSL_ERROR_WANT_WRITE:
              errno = EAGAIN;
              break;
            case SSL_ERROR_WANT_X509_LOOKUP:
              errno = EAGAIN;
              break;
          }
        }
      } else
#endif /* HAVE_SSL */
      x = write(socklist[i].sock, socklist[i].outbuf, socklist[i].outbuflen);
      if ((x < 0) && (errno != EAGAIN)
#ifdef EBADSLT
	  && (errno != EBADSLT)
#endif /* EBADSLT */
#ifdef ENOTCONN
	  && (errno != ENOTCONN)
#endif /* EBADSLT */
	) {
	/* This detects an EOF during writing */
	debug3(STR("net: eof!(write) socket %d (%s,%d)"), socklist[i].sock,
	       strerror(errno), errno);
	socklist[i].flags |= SOCK_EOFD;
      } else if (x == socklist[i].outbuflen) {
	/* If the whole buffer was sent, nuke it */
	nfree(socklist[i].outbuf);
	socklist[i].outbuf = NULL;
	socklist[i].outbuflen = 0;
      } else if (x > 0) {
	char *p = socklist[i].outbuf;

	/* This removes any sent bytes from the beginning of the buffer */
	socklist[i].outbuf = (char *) nmalloc(socklist[i].outbuflen - x);
	egg_memcpy(socklist[i].outbuf, p + x, socklist[i].outbuflen - x);
	socklist[i].outbuflen -= x;
	nfree(p);
      } else {
	debug3(STR("dequeue_sockets(): errno = %d (%s) on %d"), errno,
               strerror(errno), socklist[i].sock);
      }
      /* All queued data was sent. Call handler if one exists and the
       * dcc entry wants it.
       */
      if (!socklist[i].outbuf) {
	int idx = findanyidx(socklist[i].sock);

	if (idx > 0 && dcc[idx].type && dcc[idx].type->outdone)
	  dcc[idx].type->outdone(idx);
      }
    }
  }
}
 

/*
 *      Debugging stuff
 */

void tell_netdebug(int idx)
{
  int i;
  char s[80];

  dprintf(idx, "Open sockets:");
  for (i = 0; i < MAXSOCKS; i++) {
    if (!(socklist[i].flags & SOCK_UNUSED)) {
      sprintf(s, " %d", socklist[i].sock);
      if (socklist[i].flags & SOCK_BINARY)
	strcat(s, " (binary)");
      if (socklist[i].flags & SOCK_LISTEN)
	strcat(s, " (listen)");
      if (socklist[i].flags & SOCK_PASS)
	strcat(s, " (passed on)");
      if (socklist[i].flags & SOCK_CONNECT)
	strcat(s, " (connecting)");
      if (socklist[i].flags & SOCK_STRONGCONN)
	strcat(s, " (strong)");
      if (socklist[i].flags & SOCK_NONSOCK)
	strcat(s, " (file)");
      if (socklist[i].inbuf != NULL)
	sprintf(&s[strlen(s)], " (inbuf: %04X)", strlen(socklist[i].inbuf));
      if (socklist[i].outbuf != NULL)
	sprintf(&s[strlen(s)], " (outbuf: %06lX)", socklist[i].outbuflen);
      strcat(s, ",");
      dprintf(idx, "%s", s);
    }
  }
  dprintf(idx, " done.\n");
}

/* Security-flavoured sanity checking on DCC connections of all sorts can be
 * done with this routine.  Feed it the proper information from your DCC
 * before you attempt the connection, and this will make an attempt at
 * figuring out if the connection is really that person, or someone screwing
 * around.  It's not foolproof, but anything that fails this check probably
 * isn't going to work anyway due to masquerading firewalls, NAT routers, 
 * or bugs in mIRC.
 */
int sanitycheck_dcc(char *nick, char *from, char *ipaddy, char *port)
{
  /* According to the latest RFC, the clients SHOULD be able to handle
   * DNS names that are up to 255 characters long.  This is not broken.
   */

  /* This function needs to be updated for IPv6 IP's, although it doesn't
   * currently harm them (afaik)
   */

  char badaddress[16];
  IP ip = my_atoul(ipaddy);
  int prt = atoi(port);

  /* It is disabled HERE so we only have to check in *one* spot! */
  if (!dcc_sanitycheck)
    return 1;

  if (prt < 1) {
    putlog(LOG_MISC, "*", "ALERT: (%s!%s) specified an impossible port of %u!",
	   nick, from, prt);
    return 0;
  }
  sprintf(badaddress, "%u.%u.%u.%u", (ip >> 24) & 0xff, (ip >> 16) & 0xff,
	  (ip >> 8) & 0xff, ip & 0xff);
  if (ip < (1 << 24)) {
    putlog(LOG_MISC, "*", "ALERT: (%s!%s) specified an impossible IP of %s!",
	   nick, from, badaddress);
    return 0;
  }
  return 1;
}

int hostsanitycheck_dcc(char *nick, char *from, IP ip, char *dnsname,
		        char *prt)
{
  /* According to the latest RFC, the clients SHOULD be able to handle
   * DNS names that are up to 255 characters long.  This is not broken.
   */
  char hostn[256], badaddress[16];

  /* It is disabled HERE so we only have to check in *one* spot! */
  if (!dcc_sanitycheck)
    return 1;
  sprintf(badaddress, "%u.%u.%u.%u", (ip >> 24) & 0xff, (ip >> 16) & 0xff,
	  (ip >> 8) & 0xff, ip & 0xff);
  /* These should pad like crazy with zeros, since 120 bytes or so is
   * where the routines providing our data currently lose interest. I'm
   * using the n-variant in case someone changes that...
   */
  strncpyz(hostn, extracthostname(from), sizeof hostn);
  if (!egg_strcasecmp(hostn, dnsname)) {
    putlog(LOG_DEBUG, "*", STR("DNS information for submitted IP checks out."));
    return 1;
  }
  if (!strcmp(badaddress, dnsname))
    putlog(LOG_MISC, "*", "ALERT: (%s!%s) sent a DCC request with bogus IP "
	   "information of %s port %s. %s does not resolve to %s!", nick, from,
	    badaddress, prt, from, badaddress);
  else
    return 1; /* <- usually happens when we have 
		    a user with an unresolved hostmask! */
  return 0;
}

/* Checks wether the referenced socket has data queued.
 *
 * Returns true if the incoming/outgoing (depending on 'type') queues
 * contain data, otherwise false.
 */
int sock_has_data(int type, int sock)
{
  int ret = 0, i;

  for (i = 0; i < MAXSOCKS; i++)
    if (!(socklist[i].flags & SOCK_UNUSED) && socklist[i].sock == sock)
      break;
  if (i < MAXSOCKS) {
    switch (type) {
      case SOCK_DATA_OUTGOING:
	ret = (socklist[i].outbuf != NULL);
	break;
      case SOCK_DATA_INCOMING:
	ret = (socklist[i].inbuf != NULL);
	break;
    }
  } else
    debug1(STR("sock_has_data: could not find socket #%d, returning false."), sock);
  return ret;
}

/* flush_inbuf():
 * checks if there's data in the incoming buffer of an connection
 * and flushs the buffer if possible
 *
 * returns: -1 if the dcc entry wasn't found
 *          -2 if dcc[idx].type->activity doesn't exist and the data couldn't
 *             be handled
 *          0 if buffer was empty
 *          otherwise length of flushed buffer
 */
int flush_inbuf(int idx)
{
  int i, len;
  char *inbuf;

  Assert((idx >= 0) && (idx < dcc_total));
  for (i = 0; i < MAXSOCKS; i++) {
    if ((dcc[idx].sock == socklist[i].sock)
        && !(socklist[i].flags & SOCK_UNUSED)) {
      len = socklist[i].inbuflen;
      if ((len > 0) && socklist[i].inbuf) {
        if (dcc[idx].type && dcc[idx].type->activity) {
          inbuf = socklist[i].inbuf;
          socklist[i].inbuf = NULL;
          dcc[idx].type->activity(idx, inbuf, len);
          nfree(inbuf);
          return len;
        } else
          return -2;
      } else
        return 0;
    }
  }
  return -1;
}
