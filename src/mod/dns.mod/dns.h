#ifndef _EGG_MOD_DNS_DNS_H
#define _EGG_MOD_DNS_DNS_H
struct resolve
{
  struct resolve *next;
  struct resolve *previous;
  struct resolve *nextid;
  struct resolve *previousid;
  struct resolve *nextip;
  struct resolve *previousip;
  struct resolve *nexthost;
  struct resolve *previoushost;
  time_t expiretime;
  char *hostn;
  IP ip;
  u_16bit_t id;
  u_8bit_t state;
  u_8bit_t sends;
};
enum resolve_states
{ STATE_FINISHED, STATE_FAILED, STATE_PTRREQ, STATE_AREQ };
#define IS_PTR(x) (x->state == STATE_PTRREQ)
#define IS_A(x) (x->state == STATE_AREQ)
#ifdef DEBUG_DNS
#define ddebug0	debug0
#define ddebug1	debug1
#define ddebug2	debug2
#define ddebug3	debug3
#define ddebug4	debug4
#else
#define ddebug0(x)
#define ddebug1(x, x1)
#define ddebug2(x, x1, x2)
#define ddebug3(x, x1, x2, x3)
#define ddebug4(x, x1, x2, x3, x4)
#endif
#endif
