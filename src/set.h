#ifndef _SET_H
#define _SET_H

#include <sys/types.h>
#include "set.h"

#define var_type_name(x) (x & VAR_DETECTED) ? "detect" : (x & VAR_INT) ? "int" : (x & VAR_LIST) ? "list" : \
                         (x & VAR_STRING) ? "string" : (x & VAR_RATE) ? "rate" : (x & VAR_BOOL) ? "bool" : ""

#define VAR_INT		BIT0
#define VAR_STRING	BIT1
#define VAR_RATE	BIT2
#define VAR_BOOL	BIT3
#define VAR_HIDE	BIT4
#define VAR_DETECTED	BIT5
#define VAR_LIST	BIT6
/* send this var to server as nick? */
#define VAR_NICK	BIT7
#define VAR_SERVERS	BIT8
#define VAR_SHUFFLE	BIT9
/* no local */
#define VAR_NOLOC	BIT10	
/* no local hub */
#define VAR_NOLHUB	BIT11
/* trigger cloak script changing? */
#define VAR_CLOAK	BIT12
/* only perm owner may view/edit */
#define VAR_PERM	BIT13
/* Don't set the var data from the mem as default (NICK) */
#define VAR_NODEF	BIT14
#define VAR_CHANSET	BIT15

#define VAR_LDATA 	1
#define VAR_GDATA	2

#define set_data(x) ((x).ldata ? (x).ldata : (x).gdata ? (x).gdata : 0)
#define set_type(x) ((x).ldata ? VAR_LDATA : (x).gdata ? VAR_GDATA : 0)
#define set_types(x) ((x).ldata ? "local" : (x).gdata ? "global" : "")

typedef struct variable_b {
  const char *name;
  void *mem;
  size_t size;
  int flags;
  char *ldata;
  char *gdata;
} variable_t;

typedef struct rate_b {
 int count;
 time_t time;
} rate_t;

extern char		auth_key[], auth_prefix[2], motd[], *def_chanset,
			msgident[], msginvite[], msgop[], msgpass[], process_list[];
extern bool		dccauth;
extern int		cloak_script, fight_threshold, fork_interval, in_bots, set_noshare,
			kill_threshold, lag_threshold, op_bots, badprocess, hijack, login, promisc, trace;
extern rate_t		op_requests, close_threshold;

bool write_vars_and_cmdpass (FILE *, int);
void var_userfile_share_line(char *, int, bool);
void var_parse_my_botset();
void init_vars();
void var_set_by_name(const char *, const char *, const char *);
void var_set_userentry(const char *, const char *, const char *);
void cmd_set_real(const char *, int idx, char *);

#endif /* !_SET_H */
