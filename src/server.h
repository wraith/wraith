
/* 
 * server.h -- part of server.mod
 * 
 * $Id: server.h,v 1.7 2000/01/08 21:23:17 per Exp $
 */

/* 
 * Copyright (C) 1997  Robey Pointer
 * Copyright (C) 1999, 2000  Eggheads
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _EGG_MOD_SERVER_SERVER_H
#define _EGG_MOD_SERVER_SERVER_H

#define check_tcl_ctcp(a,b,c,d,e,f) check_tcl_ctcpr(a,b,c,d,e,f,H_ctcp)
#define check_tcl_ctcr(a,b,c,d,e,f) check_tcl_ctcpr(a,b,c,d,e,f,H_ctcr)

#ifndef MAKING_SERVER
extern char botname[],
  botuserhost[],
  newserver[],
  newserverpass[],
  ctcp_reply[];
extern int quiet_reject,
  serv,
  flud_thr,
  flud_time,
  flud_ctcp_thr,
  flud_ctcp_time;
extern int answer_ctcp,
  trigger_on_ignore,
  netserverport,
  cycle_time,
  default_port;
extern int server_online,
  min_servs,
  server_lag;
extern p_tcl_bind_list H_raw,
  H_wall,
  H_msg,
  H_msgm,
  H_notc,
  H_flud,
  H_ctcp,
  H_ctcr;
#endif

int match_my_nick(char *);
int check_tcl_flud(char *, char *, struct userrec *, char *, char *);
int check_tcl_ctcpr(char *, char *, struct userrec *, char *, char *, char *, p_tcl_bind_list);
int detect_avalanche(char *);
void nuke_server(char *);
char *get_altbotnick();

#endif /* _EGG_MOD_SERVER_SERVER_H */
