
/* 
 * net.c -- handles:
 *   all raw network i/o
 * 
 * $Id: net.c,v 1.17 2000/01/17 16:14:45 per Exp $
 */

/* 
 * This is hereby released into the public domain.
 * Robey Pointer, robey@netcom.com
 */

#include "main.h"
#include "hook.h"
#include "proto.h"
#include <limits.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>		/* is this really necessary? */
#include <errno.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <setjmp.h>

#if !HAVE_GETDTABLESIZE
#ifdef FD_SETSIZE
#define getdtablesize() FD_SETSIZE
#else
#define getdtablesize() 200
#endif
#endif

extern int backgrd,
  use_stderr,
  resolve_timeout;

char hostname[121] = "";	/* hostname can be specified in the config

				 * file */
char myip[121] = "";		/* IP can be specified in the config file */
char firewall[121] = "";	/* socks server for firewall */

#ifdef HUB
int dolookups = 0;
#else
int dolookups = 1;		/* Ghost: Default to lookup all connects */
#endif
int firewallport = 1080;	/* default port of Sock4/5 firewalls */
char botuser[21] = "nobody";	/* username of the user running the bot */
int dcc_sanitycheck = 0;	/* we should do some sanity checking on dcc

				 * connections. */
sock_list *socklist = 0;	/* enough to be safe */
int MAXSOCKS = 0;
char netpass[128] = "";

/* types of proxy */
#define PROXY_SOCKS   1
#define PROXY_SUN     2

jmp_buf alarmret;		/* env buffer for alarm() returns */

/* i need an UNSIGNED long for dcc type stuff */
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

#define my_ntohs(sh) swap_short(sh)
#define my_htons(sh) swap_short(sh)
#define my_ntohl(ln) swap_long(ln)
#define my_htonl(ln) swap_long(ln)


#ifdef G_ANTITRACE
extern struct cfg_entry CFG_TRACE;
#endif


/* i read somewhere that memcpy() is broken on some machines */

/* it's easy to replace, so i'm not gonna take any chances, because
 * it's pretty important that it work correctly here */
void my_memcpy(char *dest, char *src, int len)
{
  while (len--)
    *dest++ = *src++;
}

#ifndef HAVE_BZERO

/* bzero() is bsd-only, so here's one for non-bsd systems */
void bzero(char *dest, int len)
{
  while (len--)
    *dest++ = 0;
}
#endif

/* initialize the socklist */
void init_net()
{
  int i;

  for (i = 0; i < MAXSOCKS; i++) {
    bzero(&socklist[i], sizeof(socklist[i]));
    socklist[i].flags = SOCK_UNUSED;
  }
}

int expmem_net()
{
  int i,
    tot = 0;

  Context;
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

/* get my ip number */
IP getmyip()
{
  struct hostent *hp;
  char s[121];
  IP ip;
  struct in_addr *in;

  /* could be pre-defined */
  if (myip[0]) {
    if ((myip[strlen(myip) - 1] >= '0') && (myip[strlen(myip) - 1] <= '9'))
      return (IP) inet_addr(myip);
  }
  /* also could be pre-defined */
  if (hostname[0])
    hp = gethostbyname(hostname);
  else {
    gethostname(s, 120);

    hp = gethostbyname(s);
  }
  if (hp == NULL) {
    return inet_addr(STR("127.0.0.1"));

/*    fatal("Hostname self-lookup failed.", 0); */
  }
  in = (struct in_addr *) (hp->h_addr_list[0]);
  ip = (IP) (in->s_addr);
  return ip;
}

void neterror(char *s)
{
  switch (errno) {
  case EADDRINUSE:
    strcpy(s, STR("Address already in use"));
    break;
  case EADDRNOTAVAIL:
    strcpy(s, STR("Address invalid on remote machine"));
    break;
  case EAFNOSUPPORT:
    strcpy(s, STR("Address family not supported"));
    break;
  case EALREADY:
    strcpy(s, STR("Socket already in use"));
    break;
  case EBADF:
    strcpy(s, STR("Socket descriptor is bad"));
    break;
  case ECONNREFUSED:
    strcpy(s, STR("Connection refused"));
    break;
  case EFAULT:
    strcpy(s, STR("Namespace segment violation"));
    break;
  case EINPROGRESS:
    strcpy(s, STR("Operation in progress"));
    break;
  case EINTR:
    strcpy(s, STR("Timeout"));
    break;
  case EINVAL:
    strcpy(s, STR("Invalid namespace"));
    break;
  case EISCONN:
    strcpy(s, STR("Socket already connected"));
    break;
  case ENETUNREACH:
    strcpy(s, STR("Network unreachable"));
    break;
  case ENOTSOCK:
    strcpy(s, STR("File descriptor, not a socket"));
    break;
  case ETIMEDOUT:
    strcpy(s, STR("Connection timed out"));
    break;
  case ENOTCONN:
    strcpy(s, STR("Socket is not connected"));
    break;
  case EHOSTUNREACH:
    strcpy(s, STR("Host is unreachable"));
    break;
  case EPIPE:
    strcpy(s, STR("Broken pipe"));
    break;
#ifdef ECONNRESET
  case ECONNRESET:
    strcpy(s, STR("Connection reset by peer"));
    break;
#endif
#ifdef EACCES
  case EACCES:
    strcpy(s, STR("Permission denied"));
    break;
#endif
  case 0:
    strcpy(s, STR("Error 0"));
    break;
  default:
    sprintf(s, STR("Unforseen error %d"), errno);
    break;
  }
}

/* request a normal socket for i/o */
void setsock(int sock, int options)
{
  int i;
  int parm;

  for (i = 0; i < MAXSOCKS; i++) {
    if (socklist[i].flags & SOCK_UNUSED) {
      /* yay!  there is table space */
      socklist[i].inbuf = socklist[i].outbuf = NULL;
      socklist[i].outbuflen = 0;
      socklist[i].flags = options;
      socklist[i].sock = sock;
      socklist[i].encstatus = 0;
      bzero(&socklist[i].okey, sizeof(socklist[i].okey));
      bzero(&socklist[i].ikey, sizeof(socklist[i].ikey));
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
      /* yay async i/o ! */
      fcntl(sock, F_SETFL, O_NONBLOCK);
      return;
    }
  }
  fatal(STR("Socket table is full!"), 0);
}

int getsock(int options)
{
  int sock = socket(AF_INET, SOCK_STREAM, 0);

  if (sock < 0)
    fatal(STR("Can't open a socket at all!"), 0);
  setsock(sock, options);
  return sock;
}

/* done with a socket */
void killsock(int sock)
{
  int i;

  for (i = 0; i < MAXSOCKS; i++) {
    if ((socklist[i].sock == sock) && !(socklist[i].flags & SOCK_UNUSED)) {
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
      bzero(&socklist[i], sizeof(socklist[i]));
      socklist[i].flags = SOCK_UNUSED;
      return;
    }
  }
  log(LCAT_ERROR, STR("Attempt to kill un-allocated socket %d !!"), sock);
}

/* send connection request to proxy */
int proxy_connect(int sock, char *host, int port, int proxy)
{
  unsigned char x[10];
  struct hostent *hp;
  char s[30];

  /* socks proxy */
  if (proxy == PROXY_SOCKS) {
    /* numeric IP? */
    if ((host[strlen(host) - 1] >= '0') && (host[strlen(host) - 1] <= '9')) {
      IP ip = ((IP) inet_addr(host));	/* drummer */

      my_memcpy((char *) x, (char *) &ip, 4);	/* Beige@Efnet */
    } else {
      /* no, must be host.domain */
      if (!setjmp(alarmret)) {
	alarm(resolve_timeout);
	hp = gethostbyname(host);
	alarm(0);
      } else {
	hp = NULL;
      }
      if (hp == NULL) {
	killsock(sock);
	return -2;
      }
      my_memcpy((char *) x, (char *) hp->h_addr, hp->h_length);
    }

/*
    for (i = 0; i < MAXSOCKS; i++) {
      if (!(socklist[i].flags & SOCK_UNUSED) && (socklist[i].sock == sock))
	socklist[i].flags |= SOCK_PROXYWAIT;
    }
*/
    sprintf(s, STR("\004\001%c%c%c%c%c%c%s"), (port >> 8) % 256, (port % 256), x[0], x[1], x[2], x[3], botuser);
    tputs(sock, s, strlen(botuser) + 9);	/* drummer */
    read(sock, s, 10);

  } else if (proxy == PROXY_SUN) {
    sprintf(s, STR("%s %d\n"), host, port);
    tputs(sock, s, strlen(s));	/* drummer */
  }
  return sock;
}


/* starts a connection attempt to a socket
 * returns <0 if connection refused:
 *   -1  neterror() type error
 *   -2  can't resolve hostname */
int open_telnet_raw(int sock, char *server, int sport)
{
  struct sockaddr_in name;
  struct hostent *hp;
  char host[121];
  int i,
    port,
    socks = 0;
  volatile int proxy;
  char *hpart;

/* If server matches *?*, use first as socks and second as server */
  hpart = strchr(server, '?');
  if (hpart) {
    strcpy(host, server);
    *strchr(host, '?') = 0;
    hpart += 1;
    strcpy(server, hpart);
    socks = 1;
    proxy = PROXY_SOCKS;
    port = 1080;
  } else {
    proxy = 0;
    strcpy(host, server);
    port = sport;
  }

  /* patch by tris for multi-hosted machines: */
  bzero((char *) &name, sizeof(struct sockaddr_in));

  name.sin_family = AF_INET;
  name.sin_addr.s_addr = (myip[0] ? getmyip() : INADDR_ANY);
  if (bind(sock, (struct sockaddr *) &name, sizeof(name)) < 0) {
    log(LCAT_ERROR, STR("Error binding to ip: %s\n"), strerror(errno));
    return -1;
  }
  bzero((char *) &name, sizeof(struct sockaddr_in));

  name.sin_family = AF_INET;
  name.sin_port = my_htons(port);

  /* numeric IP? */
  if ((host[strlen(host) - 1] >= '0') && (host[strlen(host) - 1] <= '9'))
    name.sin_addr.s_addr = inet_addr(host);
  else {
    /* no, must be host.domain */
    if (!setjmp(alarmret)) {
      alarm(resolve_timeout);
      hp = gethostbyname(host);
      alarm(0);
    } else {
      hp = NULL;
    }
    if (hp == NULL)
      return -2;
    my_memcpy((char *) &name.sin_addr, hp->h_addr, hp->h_length);
    name.sin_family = hp->h_addrtype;
  }
  for (i = 0; i < MAXSOCKS; i++) {
    if (!(socklist[i].flags & SOCK_UNUSED) && (socklist[i].sock == sock))
      socklist[i].flags |= SOCK_CONNECT;
  }
  if (connect(sock, (struct sockaddr *) &name, sizeof(struct sockaddr_in)) < 0) {
    if (errno == EINPROGRESS) {
      /* firewall?  announce connect attempt to proxy */
      if (proxy) {
	return proxy_connect(sock, server, sport, proxy);
      }
      return sock;		/* async success! */
    } else
      return -1;
  }
  /* synchronous? :/ */
  if (proxy)
    return proxy_connect(sock, server, sport, proxy);
  return sock;
}

/* ordinary non-binary connection attempt */
int open_telnet(char *server, int port)
{
  int sock = getsock(0),
    ret = open_telnet_raw(sock, server, port);

  if (ret < 0)
    killsock(sock);
  return ret;
}

/* returns a socket number for a listening socket that will accept any
 * connection -- port # is returned in port */
int open_listen(int *port)
{
  int sock;
  unsigned int addrlen;
  struct sockaddr_in name;

  if (firewall[0]) {
    /* FIXME: can't do listen port thru firewall yet */
    log(LCAT_ERROR, STR("!! Cant open a listen port (you are using a firewall)"));
    return -1;
  }
  sock = getsock(SOCK_LISTEN);
  bzero((char *) &name, sizeof(struct sockaddr_in));

  name.sin_family = AF_INET;
  name.sin_port = my_htons(*port);	/* 0 = just assign us a port */
  name.sin_addr.s_addr = (myip[0] ? getmyip() : INADDR_ANY);
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
  *port = my_ntohs(name.sin_port);
  if (listen(sock, 1) < 0) {
    killsock(sock);
    return -1;
  }
  return sock;
}

/* given network-style IP address, return hostname */

/* hostname will be "##.##.##.##" format if there was an error */
char *hostnamefromip(unsigned long ip)
{
  struct hostent *hp;
  unsigned long addr = ip;
  unsigned char *p;
  static char s[121];

  if (!setjmp(alarmret)) {
    alarm(resolve_timeout);
    hp = gethostbyaddr((char *) &addr, sizeof(addr), AF_INET);
    alarm(0);
  } else {
    hp = NULL;
  }
  if (hp == NULL) {
    p = (unsigned char *) &addr;
    sprintf(s, STR("%u.%u.%u.%u"), p[0], p[1], p[2], p[3]);
    return s;
  }
  strncpy0(s, hp->h_name, 120);
  return s;
}

char *stringip(unsigned long ip)
{
  unsigned long addr = ip;
  unsigned char *p;
  static char s[121];

  p = (unsigned char *) &addr;
  sprintf(s, STR("%u.%u.%u.%u"), p[0], p[1], p[2], p[3]);
  return s;
}

/* short routine to answer a connect received on a socket made previously
 * by open_listen ... returns hostname of the caller & the new socket
 * does NOT dispose of old "public" socket! */
int answer(int sock, char *caller, unsigned long *ip, unsigned short *port, int binary)
{
  int new_sock;
  unsigned int addrlen;
  struct sockaddr_in from;
  addrlen = sizeof(struct sockaddr);

  new_sock = accept(sock, (struct sockaddr *) &from, &addrlen);
  if (new_sock < 0)
    return -1;
  if (ip != NULL) {
    *ip = from.sin_addr.s_addr;
    if (dolookups) {
      strncpy0(caller, hostnamefromip(*ip), 120);
    } else {
      strncpy0(caller, stringip(*ip), 120);
    }
    *ip = my_ntohl(*ip);
  }
  if (port != NULL)
    *port = my_ntohs(from.sin_port);
  /* set up all the normal socket crap */
  setsock(new_sock, (binary ? SOCK_BINARY : 0));
  return new_sock;
}

/* like open_telnet, but uses server & port specifications of dcc */
int open_telnet_dcc(int sock, char *server, char *port)
{
  int p;
  unsigned long addr;
  char sv[121];
  unsigned char c[4];

  if (port != NULL)
    p = atoi(port);
  else
    p = 2000;
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
  sprintf(sv, STR("%u.%u.%u.%u"), c[0], c[1], c[2], c[3]);
  /* strcpy(sv,hostnamefromip(addr)); */
  p = open_telnet_raw(sock, sv, p);
  return p;
}

/* all new replacements for mtgets/mtread */

/* attempts to read from all the sockets in socklist
 * fills s with up to 511 bytes if available, and returns the array index
 * on EOF, returns -1, with socket in len
 * on socket error, returns -2
 * if nothing is ready, returns -3 */
int sockread(char *s, int *len)
{
  fd_set fd;
  int fds,
    i,
    x;
  struct timeval t;
  int grab = 511;

  fds = getdtablesize();
#ifdef FD_SETSIZE
  if (fds > FD_SETSIZE)
    fds = FD_SETSIZE;		/* fixes YET ANOTHER freebsd bug!!! */
#endif
  /* timeout: 1 sec */
  t.tv_sec = 1;
  t.tv_usec = 0;
  FD_ZERO(&fd);
  for (i = 0; i < MAXSOCKS; i++)
    if (!(socklist[i].flags & SOCK_UNUSED)) {
      if ((socklist[i].sock == STDOUT) && !backgrd)
	FD_SET(STDIN, &fd);
      else
	FD_SET(socklist[i].sock, &fd);
    }
#ifdef HPUX_HACKS
#ifndef HPUX10_HACKS
  x = select(fds, (int *) &fd, (int *) NULL, (int *) NULL, &t);
#else
  x = select(fds, &fd, NULL, NULL, &t);
#endif
#else
  x = select(fds, &fd, NULL, NULL, &t);
#endif
  if (x > 0) {
    /* something happened */
    for (i = 0; i < MAXSOCKS; i++) {
      if ((!(socklist[i].flags & SOCK_UNUSED)) && ((FD_ISSET(socklist[i].sock, &fd)) || ((socklist[i].sock == STDOUT) && (!backgrd) && (FD_ISSET(STDIN, &fd))))) {
	if (socklist[i].flags & (SOCK_LISTEN | SOCK_CONNECT)) {
	  /* listening socket -- don't read, just return activity */
	  /* same for connection attempt */
	  /* (for strong connections, require a read to succeed first) */
	  if (socklist[i].flags & SOCK_PROXYWAIT) {	/* drummer */
	    /* hang around to get the return code from proxy */
	    grab = 10;
	  } else if (!(socklist[i].flags & SOCK_STRONGCONN)) {
	    debug1(STR("net: connect! sock %d"), socklist[i].sock);
	    s[0] = 0;
	    *len = 0;
	    return i;
	  }
	}
	if ((socklist[i].sock == STDOUT) && !backgrd)
	  x = read(STDIN, s, grab);
	else
	  x = read(socklist[i].sock, s, grab);
	if (x <= 0) {		/* eof */
	  if (x == EAGAIN) {
	    s[0] = 0;
	    *len = 0;
	    return -3;
	  }
	  *len = socklist[i].sock;
	  socklist[i].flags &= ~SOCK_CONNECT;
	  debug1(STR("net: eof!(read) socket %d"), socklist[i].sock);
	  return -1;
	}
	s[x] = 0;
	*len = x;
	if (socklist[i].flags & SOCK_PROXYWAIT) {
	  debug2(STR("net: socket: %d proxy errno: %d"), socklist[i].sock, s[1]);
	  socklist[i].flags &= ~(SOCK_CONNECT | SOCK_PROXYWAIT);
	  switch (s[1]) {
	  case 90:		/* success */
	    s[0] = 0;
	    *len = 0;
	    return i;
	  case 91:		/* failed */
	    errno = ECONNREFUSED;
	    break;
	  case 92:		/* no identd */
	  case 93:		/* identd said wrong username */
	    errno = ENETUNREACH;
	    break;
	    /* a better error message would be "socks misconfigured" */
	    /* or "identd not working" but this is simplest */
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
  char *srcbuf = NULL,
   *buf = NULL,
   *line = NULL,
   *eol = NULL,
   *eline = NULL;
  int bufpos = 0,
    i;

  srcbuf = nmalloc(strlen(src) + 10);
  strcpy(srcbuf, src);
  line = srcbuf;
  if (!line)
    return NULL;

  eol = strchr(line, '\n');
  while (eol) {
    *eol++ = 0;
    eline = encrypt_string(socklist[snum].okey, line);
    if (socklist[snum].oseed) {
      for (i = 0; i <= 3; i++)
	*(dword *) & socklist[snum].okey[i * 4] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      if (!socklist[snum].oseed)
	socklist[snum].oseed++;
    }
    buf = nrealloc(buf, bufpos + strlen(eline) + 10);
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
      for (i = 0; i <= 3; i++)
	*(dword *) & socklist[snum].okey[i * 4] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      if (!socklist[snum].oseed)
	socklist[snum].oseed++;
    }
    buf = nrealloc(buf, bufpos + strlen(eline) + 10);
    strcpy((char *) &buf[bufpos], eline);
    strcat(buf, "\n");
    nfree(eline);
  }
  nfree(srcbuf);
  return buf;
}

/* sockgets: buffer and read from sockets
 * 
 * attempts to read from all registered sockets for up to one second.  if
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
 */

int sockgets(char *s, int *len)
{
  char xx[514],
   *p,
   *px;
  int ret,
    i,
    data = 0;

  Context;
  for (i = 0; i < MAXSOCKS; i++) {
    /* check for stored-up data waiting to be processed */
    if (!(socklist[i].flags & SOCK_UNUSED) && (socklist[i].inbuf != NULL)) {
      /* look for \r too cos windows can't follow RFCs */
      p = strchr(socklist[i].inbuf, '\n');
      if (p == NULL)
	p = strchr(socklist[i].inbuf, '\r');
      if (p != NULL) {
	*p = 0;
	if (strlen(socklist[i].inbuf) > 510)
	  socklist[i].inbuf[510] = 0;
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
	/* strip CR if this was CR/LF combo */
	if (s[strlen(s) - 1] == '\r')
	  s[strlen(s) - 1] = 0;
	if (socklist[i].encstatus)
	  botlink_decrypt(i, s);
	*len = strlen(s);
	return socklist[i].sock;
      }
    }
    /* also check any sockets that might have EOF'd during write */
    if (!(socklist[i].flags & SOCK_UNUSED)
	&& (socklist[i].flags & SOCK_EOFD)) {
      Context;
      s[0] = 0;
      *len = socklist[i].sock;
      return -1;
    }
  }
  /* no pent-up data of any worth -- down to business */
  Context;
  *len = 0;
  ret = sockread(xx, len);
  if (ret < 0) {
    s[0] = 0;
    return ret;
  }
  /* binary and listening sockets don't get buffered */
  if (socklist[ret].flags & SOCK_CONNECT) {
    if (socklist[ret].flags & SOCK_STRONGCONN) {
      socklist[ret].flags &= ~SOCK_STRONGCONN;
      /* buffer any data that came in, for future read */
      socklist[ret].inbuf = (char *) nmalloc(strlen(xx) + 1);
      strcpy(socklist[ret].inbuf, xx);
    }
    socklist[ret].flags &= ~SOCK_CONNECT;
    s[0] = 0;
    return socklist[ret].sock;
  }
  if (socklist[ret].flags & SOCK_BINARY) {
    my_memcpy(s, xx, *len);
    return socklist[ret].sock;
  }
  if (socklist[ret].flags & SOCK_LISTEN)
    return socklist[ret].sock;
  Context;
  /* might be necessary to prepend stored-up data! */
  if (socklist[ret].inbuf != NULL) {
    p = socklist[ret].inbuf;
    socklist[ret].inbuf = (char *) nmalloc(strlen(p) + strlen(xx) + 1);
    strcpy(socklist[ret].inbuf, p);
    strcat(socklist[ret].inbuf, xx);
    nfree(p);
    if (strlen(socklist[ret].inbuf) < 512) {
      strcpy(xx, socklist[ret].inbuf);
      nfree(socklist[ret].inbuf);
      socklist[ret].inbuf = NULL;
    } else {
      p = socklist[ret].inbuf;
      socklist[ret].inbuf = (char *) nmalloc(strlen(p) - 509);
      strcpy(socklist[ret].inbuf, p + 510);
      *(p + 510) = 0;
      strcpy(xx, p);
      nfree(p);
      /* (leave the rest to be post-pended later) */
    }
  }
  Context;
  /* look for EOL marker; if it's there, i have something to show */
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
				 * blank line */

/* NO! */

/* if (!s[0]) strcpy(s," ");  */
  } else {
    s[0] = 0;
    if (strlen(xx) >= 510) {
      /* string is too long, so just insert fake \n */
      strcpy(s, xx);
      xx[0] = 0;
      data = 1;
    }
  }
  Context;
  if (socklist[ret].encstatus)
    botlink_decrypt(ret, s);
  *len = strlen(s);
  /* anything left that needs to be saved? */
  if (!xx[0]) {
    if (data)
      return socklist[ret].sock;
    else
      return -3;
  }
  Context;
  /* prepend old data back */
  if (socklist[ret].inbuf != NULL) {
    Context;
    p = socklist[ret].inbuf;
    socklist[ret].inbuf = (char *) nmalloc(strlen(p) + strlen(xx) + 1);
    strcpy(socklist[ret].inbuf, xx);
    strcat(socklist[ret].inbuf, p);
    nfree(p);
  } else {
    Context;
    socklist[ret].inbuf = (char *) nmalloc(strlen(xx) + 1);
    strcpy(socklist[ret].inbuf, xx);
  }
  Context;
  if (data) {
    Context;
    return socklist[ret].sock;
  } else {
    Context;
    return -3;
  }
}

/* dump something to a socket */

/* DO NOT PUT CONTEXTS IN HERE IF YOU WANT DEBUG TO BE MEANINGFUL!!! */
void tputs(register int z, char *s, unsigned int len)
{
  register int i,
    x;
  char *p;
  static int inhere = 0;

  if (z < 0)
    return;			/* um... HELLO?!  sanity check please! */
  if (((z == STDOUT) || (z == STDERR)) && (!backgrd || use_stderr)) {
    write(z, s, len);
    return;
  }
  for (i = 0; i < MAXSOCKS; i++) {
    if (!(socklist[i].flags & SOCK_UNUSED) && (socklist[i].sock == z)) {
      if (socklist[i].encstatus) {
	if ((!s) || (!s[0])) {
	  s = botlink_encrypt(i, s);
	  len = strlen(s);
	}
	s = botlink_encrypt(i, s);
	len = strlen(s);
      }
      if (socklist[i].outbuf != NULL) {
	/* already queueing: just add it */
	p = (char *) nrealloc(socklist[i].outbuf, socklist[i].outbuflen + len);
	my_memcpy(p + socklist[i].outbuflen, s, len);
	socklist[i].outbuf = p;
	socklist[i].outbuflen += len;
	if (socklist[i].encstatus)
	  nfree(s);
	return;
      }
      /* try. */
      x = write(z, s, len);
      if (x == (-1))
	x = 0;
      if (x < len) {
	/* socket is full, queue it */
	socklist[i].outbuf = (char *) nmalloc(len - x);
	my_memcpy(socklist[i].outbuf, &s[x], len - x);
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
    log(LCAT_ERROR, STR("!!! writing to nonexistent socket: %d"), z);
    s[strlen(s) - 1] = 0;
    log(LCAT_ERROR, STR("!-> '%s'"), s);
    inhere = 0;
  }
  if (socklist[i].encstatus)
    nfree(s);
}

/* tputs might queue data for sockets, let's dump as much of it as
 * possible */
void dequeue_sockets()
{
  int i,
    x;

  for (i = 0; i < MAXSOCKS; i++) {
    if (!(socklist[i].flags & SOCK_UNUSED) && (socklist[i].outbuf != NULL)) {
      /* trick tputs into doing the work */
      x = write(socklist[i].sock, socklist[i].outbuf, socklist[i].outbuflen);
      if ((x < 0) && (errno != EAGAIN)
#ifdef EBADSLT
	  && (errno != EBADSLT)
#endif
#ifdef ENOTCONN
	  && (errno != ENOTCONN)
#endif
	) {
	/* this detects an EOF during writing */
	debug3(STR("net: eof!(write) socket %d (%s,%d)"), socklist[i].sock, strerror(errno), errno);
	socklist[i].flags |= SOCK_EOFD;
      } else if (x == socklist[i].outbuflen) {
	/* if the whole buffer was sent, nuke it */
	nfree(socklist[i].outbuf);
	socklist[i].outbuf = NULL;
	socklist[i].outbuflen = 0;
      } else if (x > 0) {
	char *p = socklist[i].outbuf;

	/* this removes any sent bytes from the beginning of the buffer */
	socklist[i].outbuf = (char *) nmalloc(socklist[i].outbuflen - x);
	my_memcpy(socklist[i].outbuf, p + x, socklist[i].outbuflen - x);
	socklist[i].outbuflen -= x;
	nfree(p);
      }
    }
  }
}

/* DEBUGGING STUFF */

void tell_netdebug(int idx)
{
  int i;
  char s[80];

  dprintf(idx, STR("Open sockets:"));
  for (i = 0; i < MAXSOCKS; i++) {
    if (!(socklist[i].flags & SOCK_UNUSED)) {
      sprintf(s, STR(" %d"), socklist[i].sock);
      if (socklist[i].flags & SOCK_BINARY)
	strcat(s, STR(" (binary)"));
      if (socklist[i].flags & SOCK_LISTEN)
	strcat(s, STR(" (listen)"));
      if (socklist[i].flags & SOCK_CONNECT)
	strcat(s, STR(" (connecting)"));
      if (socklist[i].flags & SOCK_STRONGCONN)
	strcat(s, STR(" (strong)"));
      if (socklist[i].flags & SOCK_NONSOCK)
	strcat(s, STR(" (file)"));
      if (socklist[i].inbuf != NULL)
	sprintf(&s[strlen(s)], STR(" (inbuf: %04X)"), strlen(socklist[i].inbuf));
      if (socklist[i].outbuf != NULL)
	sprintf(&s[strlen(s)], STR(" (outbuf: %06lX)"), socklist[i].outbuflen);
      strcat(s, ",");
      dprintf(idx, "%s", s);
    }
  }
  dprintf(idx, STR(" done.\n"));
}

/* Security-flavoured sanity checking on DCC connections of all sorts can be
 * done with this routine.  Feed it the proper information from your DCC
 * before you attempt the connection, and this will make an attempt at
 * figuring out if the connection is really that person, or someone screwing
 * around.  It's not foolproof, but anything that fails this check probably
 * isn't going to work anyway due to masquerading firewalls, NAT routers, 
 * or bugs in mIRC. */
int sanitycheck_dcc(char *nick, char *from, char *ipaddy, char *port)
{
  /* According to the latest RFC, the clients SHOULD be able to handle
   * DNS names that are up to 255 characters long.  This is not broken.  */
  char hostname[256],
    dnsname[256],
    badaddress[16];
  IP ip = my_atoul(ipaddy);
  int prt = atoi(port);

  /* It is disabled HERE so we only have to check in *one* spot! */
  if (!dcc_sanitycheck)
    return 1;
  Context;			/* This should be pretty solid, but
				 * something _might_ break. */
  sprintf(badaddress, STR("%u.%u.%u.%u"), (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
  if (prt < 1) {
    log(LCAT_WARNING, STR("ALERT: (%s!%s) specified an impossible port of %u!"), nick, from, prt);
    return 0;
  }
  if (ip < (1 << 24)) {
    log(LCAT_WARNING, STR("ALERT: (%s!%s) specified an impossible IP of %s!"), nick, from, badaddress);
    return 0;
  }
  /* These should pad like crazy with zeros, since 120 bytes or so is
   * where the routines providing our data currently lose interest. I'm
   * using the n-variant in case someone changes that... */
  strncpy0(hostname, extracthostname(from), 255);
  /* But if they are changed one day, this might crash 
   * without [256] = 0; ++rtc
   */
  strncpy0(dnsname, hostnamefromip(my_htonl(ip)), 255);
  if (!strcasecmp(hostname, dnsname)) {
    log(LCAT_DEBUG, STR("DNS information for submitted IP checks out."));
    return 1;
  }
  if (!strcmp(badaddress, dnsname))
    log(LCAT_WARNING, STR("ALERT: (%s!%s) sent a DCC request with bogus IP information of %s port %u!"), nick, from, badaddress, prt);
  else
    return 1;			/* <- usually happens when we have 
				   a user with an unresolved hostmask! */
  return 0;
}
