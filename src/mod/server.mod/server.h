#ifdef LEAF
/*
 * server.h -- part of server.mod
 *
 */

#ifndef _EGG_MOD_SERVER_SERVER_H
#define _EGG_MOD_SERVER_SERVER_H

#define DO_LOST 1
#define NO_LOST 0

#define check_bind_ctcp(a,b,c,d,e,f) check_bind_ctcpr(a,b,c,d,e,f,BT_ctcp)
#define check_bind_ctcr(a,b,c,d,e,f) check_bind_ctcpr(a,b,c,d,e,f,BT_ctcr)

#define fixcolon(x)             do {                                    \
        if ((x)[0] == ':')                                              \
                (x)++;                                                  \
        else                                                            \
                (x) = newsplit(&(x));                                   \
} while (0)


#ifndef MAKING_SERVER
/* 4 - 7 */
/* UNUSED 5 */
#define quiet_reject (*(int *)(server_funcs[6]))
/* UNUSED 7 */
#define servi (*(int *)(server_funcs[7]))
/* 8 - 11 */
#define flud_thr (*(int*)(server_funcs[8]))
#define flud_time (*(int*)(server_funcs[9]))
#define flud_ctcp_thr (*(int*)(server_funcs[10]))
#define flud_ctcp_time (*(int*)(server_funcs[11]))
/* 12 - 15 */
#define match_my_nick ((int(*)(char *))server_funcs[12])
/* UNUSED 14 */
#define answer_ctcp (*(int *)(server_funcs[15]))
/* 16 - 19 */
#define trigger_on_ignore (*(int *)(server_funcs[16]))
#define check_bind_ctcpr ((int(*)(char*,char*,struct userrec*,char*,char*,char*,bind_table_t *))server_funcs[17])
#define detect_avalanche ((int(*)(char *))server_funcs[18])
/* 19 UNUSED */
/* 20 - 22 */
/* UNUSED 20 */
/* UNUSED 21 */
/* UNUSED 22 */
/* 23 - 26 */
/* UNUSED 23 */
/* UNUSED 24 */
/* UNUSED 25 */
/* 26 -- UNUSED */
/* 27 - 30 */
/* UNUSED -- 28 */
/* UNUSED -- 29 */
/* UNUSED -- 30 */
/* 31 - 34 */
/* UNUSED -- 33 */
/* UNUSED -- 34 */
/* 35 - 38 */
/* UNUSED 35 */
/* 36 UNUSED */
#define nick_len (*(int *)(server_funcs[37]))
/* 38 UNUSED */
#define checked_hostmask (*(int *)(server_funcs[39]))

#else		/* MAKING_SERVER */

/* Macros for commonly used commands.
 */

#define free_null(ptr)	do {				\
	free(ptr);					\
	ptr = NULL;					\
} while (0)

#define write_to_server(x,y) do {                       \
        tputs(serv, (x), (y));                          \
        tputs(serv, "\r\n", 2);                         \
} while (0)


#endif		/* MAKING_SERVER */

struct server_list {
  struct server_list	*next;

  char			*name;
  int			 port;
  char			*pass;
  char			*realname;
};

/* Available net types.  */
enum {
	NETT_EFNET		= 0,	/* EfNet except new +e/+I hybrid. */
	NETT_IRCNET		= 1,	/* Ircnet.			  */
	NETT_UNDERNET		= 2,	/* Undernet.			  */
	NETT_DALNET		= 3,	/* Dalnet.			  */
	NETT_HYBRID_EFNET	= 4	/* new +e/+I Efnet hybrid.	  */
} nett_t;

#define IRC_CANTCHANGENICK "Can't change nickname on %s.  Is my nickname banned?"
#endif		/* _EGG_MOD_SERVER_SERVER_H */

void nuke_server(char *);
extern int 		serv, servidx, cycle_time, default_port, newserverport;
extern time_t		server_online;
extern char		cursrvname[], botrealname[], botuserhost[], ctcp_reply[],
			newserver[], newserverpass[];

#endif /*leaf*/
