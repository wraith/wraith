/*
 * dns.h
 *   stuff used by dns.c
 *
 */

#ifndef _EGG_DNS_H
#define _EGG_DNS_H

#include "types.h"
#include "dcc.h"

/* Flags for dns_type
 */
#define RES_HOSTBYIP  1         /* hostname to IP address               */
#define RES_IPBYHOST  2         /* IP address to hostname               */

struct dns_info {
  void (*dns_success)(int);     /* is called if the dns request succeeds   */
  void (*dns_failure)(int);     /* is called if it fails                   */
  char *host;                   /* hostname                                */
  char *cbuf;                   /* temporary buffer. Memory will be free'd
                                   as soon as dns_info is free'd           */
  char *cptr;                   /* temporary pointer                       */
  IP ip;                        /* IP address                              */
  int ibuf;                     /* temporary buffer for one integer        */
  char dns_type;                /* lookup type, e.g. RES_HOSTBYIP          */
  struct dcc_table *type;       /* type of the dcc table we are making the
                                   lookup for                              */
};

typedef struct {
  char *name;
  void (*event)(IP, char *, int, void *);
} devent_type;

typedef struct {
  char *proc;			/* Tcl proc			  */
  char *paras;			/* Additional parameters	  */
} devent_tclinfo_t;

typedef struct devent_str {
  struct devent_str *next;	/* Pointer to next dns_event	  */
  devent_type	*type;
  u_8bit_t	lookup;		/* RES_IPBYHOST or RES_HOSTBYIP	  */
  union {
    IP		ip_addr;	/* IP address			  */
    char	*hostname; 	/* Hostname			  */
  } res_data;
  void		*other;		/* Data specific to the event type */
} devent_t;

void block_dns_hostbyip(IP);
void block_dns_ipbyhost(char *);
void call_hostbyip(IP, char *, int);
void call_ipbyhost(char *, IP, int);
void dcc_dnshostbyip(IP);
void dcc_dnsipbyhost(char *);


#endif	/* _EGG_DNS_H */
