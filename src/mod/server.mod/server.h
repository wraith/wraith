/*
 * server.h -- part of server.mod
 *
 */

#ifndef _EGG_MOD_SERVER_SERVER_H
#define _EGG_MOD_SERVER_SERVER_H

#include "src/tclhash.h"

#define DO_LOST 1
#define NO_LOST 0

#define fixcolon(x)             do {                                    \
        if ((x)[0] == ':')                                              \
                (x)++;                                                  \
        else                                                            \
                (x) = newsplit(&(x));                                   \
} while (0)
#define write_to_server(x,y) do {                       \
        tputs(serv, (x), (y));                          \
        tputs(serv, "\r\n", 2);                         \
} while (0)


struct server_list {
  struct server_list	*next;
  char			*name;
  char			*pass;
  char			*realname;
  port_t		 port;
};

/* Available net types.  */
enum {
	NETT_EFNET		= 0,	/* EfNet except new +e/+I hybrid. */
	NETT_IRCNET		= 1,	/* Ircnet.			  */
	NETT_UNDERNET		= 2,	/* Undernet.			  */
	NETT_DALNET		= 3,	/* Dalnet.			  */
	NETT_HYBRID_EFNET	= 4	/* new +e/+I Efnet hybrid.	  */
};

#define IRC_CANTCHANGENICK "Can't change nickname on %s.  Is my nickname banned?"
#endif		/* _EGG_MOD_SERVER_SERVER_H */

extern bind_table_t	*BT_ctcp, *BT_ctcr, *BT_msgc;
extern size_t		nick_len;
extern bool		quiet_reject, trigger_on_ignore, floodless;
extern int 		servidx, ctcp_mode, flud_thr, flud_ctcp_thr, answer_ctcp, serv;
extern port_t		default_port, newserverport;
extern time_t		server_online, cycle_time, flud_time, flud_ctcp_time;
extern char		cursrvname[], botrealname[], botuserhost[], ctcp_reply[],
			newserver[], newserverpass[], curnetwork[], botuserip[];
extern struct server_list *serverlist;

int check_bind_ctcpr(char *, char *, struct userrec *, char *, char *, char *, bind_table_t *);

#define check_bind_ctcp(a, b, c, d, e, f) check_bind_ctcpr(a, b, c, d, e, f, BT_ctcp)
#define check_bind_ctcr(a, b, c, d, e, f) check_bind_ctcpr(a, b, c, d, e, f, BT_ctcr)

bool detect_avalanche(char *);
void server_report(int, int);
void server_init();
void queue_server(int, char *, int);
void server_die();
void add_server(char *);
void clearq(struct server_list *);
void nuke_server(const char *);
bool match_my_nick(char *);

