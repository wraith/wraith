#ifndef _EGG_DNS_H
#define _EGG_DNS_H
typedef struct
{
  char *name;
  int (*expmem) (void *);
  void (*event) (IP, char *, int, void *);
} devent_type;
typedef struct
{
  char *proc;
  char *paras;
} devent_tclinfo_t;
typedef struct devent_str
{
  struct devent_str *next;
  devent_type *type;
  u_8bit_t lookup;
  union
  {
    IP ip_addr;
    char *hostname;
  } res_data;
  void *other;
} devent_t;
#endif
