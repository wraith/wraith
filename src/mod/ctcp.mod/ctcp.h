/*
 * ctcp.h -- part of ctcp.mod
 *   all the defines for ctcp.c
 *
 */

#ifndef _EGG_MOD_CTCP_CTCP_H
#define _EGG_MOD_CTCP_CTCP_H

#define CLIENTINFO "SED VERSION CLIENTINFO USERINFO ERRMSG FINGER TIME ACTION DCC UTC PING ECHO  :Use CLIENTINFO <COMMAND> to get more specific information"
#define CLIENTINFO_SED "SED contains simple_encrypted_data"
#define CLIENTINFO_VERSION "VERSION shows client type, version and environment"
#define CLIENTINFO_CLIENTINFO "CLIENTINFO gives information about available CTCP commands"
#define CLIENTINFO_USERINFO "USERINFO returns user settable information"
#define CLIENTINFO_ERRMSG "ERRMSG returns error messages"
#define CLIENTINFO_FINGER "FINGER shows real name, login name and idle time of user"
#define CLIENTINFO_TIME "TIME tells you the time on the user's host"
#define CLIENTINFO_ACTION "ACTION contains action descriptions for atmosphere"
#define CLIENTINFO_DCC "DCC requests a direct_client_connection"
#define CLIENTINFO_UTC "UTC substitutes the local timezone"
#define CLIENTINFO_PING "PING returns the arguments it receives"
#define CLIENTINFO_ECHO "ECHO returns the arguments it receives"


#define CLOAK_COUNT             10 /* The number of scripts currently existing */
#define CLOAK_PLAIN             1 /* This is your plain bitchx client behaviour */
#define CLOAK_CRACKROCK         2
#define CLOAK_NEONAPPLE         3
#define CLOAK_TUNNELVISION      4
#define CLOAK_ARGON             5
#define CLOAK_EVOLVER           6
#define CLOAK_PREVAIL           7
#define CLOAK_CYPRESS           8 /* Now with full theme and customization support */
#define CLOAK_MIRC              9

void ctcp_init();
void scriptchanged();
extern char		kickprefix[], bankickprefix[];
extern bool		first_ctcp_check;

#endif				/* _EGG_MOD_CTCP_CTCP_H */
