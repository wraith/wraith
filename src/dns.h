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
  struct dcc_table *type;       /* type of the dcc table we are making the
                                   lookup for                              */
  in_addr_t ip;                        /* IP address                              */
  int ibuf;                     /* temporary buffer for one integer        */
  void (*dns_success)(int);     /* is called if the dns request succeeds   */
  void (*dns_failure)(int);     /* is called if it fails                   */
  char *host;                   /* hostname                                */
  char *cbuf;                   /* temporary buffer. Memory will be free'd
                                   as soon as dns_info is free'd           */
  char *cptr;                   /* temporary pointer                       */
  char dns_type;                /* lookup type, e.g. RES_HOSTBYIP          */
};

typedef struct {
  char *name;
  void (*event)(in_addr_t, char *, int, void *);
} devent_type;

typedef struct {
  char *proc;			/* Tcl proc			  */
  char *paras;			/* Additional parameters	  */
} devent_tclinfo_t;

typedef struct devent_str {
  struct devent_str *next;	/* Pointer to next dns_event	  */
  devent_type	*type;
  union {
    in_addr_t		ip_addr;	/* IP address			  */
    char	*hostname; 	/* Hostname			  */
  } res_data;
  void		*other;		/* Data specific to the event type */
  u_8bit_t	lookup;		/* RES_IPBYHOST or RES_HOSTBYIP	  */
} devent_t;

void block_dns_hostbyip(in_addr_t);
void block_dns_ipbyhost(char *);
void call_hostbyip(in_addr_t, char *, int);
void call_ipbyhost(char *, in_addr_t, int);
void dcc_dnshostbyip(in_addr_t);
void dcc_dnsipbyhost(char *);


#endif	/* _EGG_DNS_H */
