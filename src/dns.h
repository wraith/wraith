/*
 * dns.h
 *   stuff used by dns.c
 *
 */

#ifndef _EGG_DNS_H
#define _EGG_DNS_H

typedef struct {
  char *name;
  int  (*expmem)(void *);
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

#endif	/* _EGG_DNS_H */
