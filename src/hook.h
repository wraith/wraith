#define HOOK_GET_FLAGREC	  0
#define HOOK_BUILD_FLAGREC	  1
#define HOOK_SET_FLAGREC	  2
#define HOOK_READ_USERFILE	  3
#define HOOK_REHASH		  4
#define HOOK_MINUTELY		  5
#define HOOK_DAILY		  6
#define HOOK_HOURLY		  7
#define HOOK_USERFILE		  8
#define HOOK_SECONDLY		  9
#define HOOK_PRE_REHASH		 10
#define HOOK_IDLE		 11
#define HOOK_5MINUTELY		 12
#define HOOK_3SECONDLY		 13
#define HOOK_5SECONDLY		 14
#define HOOK_10SECONDLY		 15
#define HOOK_20SECONDLY		 16
#define HOOK_30SECONDLY		 17
#define REAL_HOOKS		 18
#define HOOK_SHAREOUT		105
#define HOOK_SHAREIN		106
#define DEL_HOOK_ENCRYPT_PASS	107
#define HOOK_QSERV		108
#define HOOK_MATCH_NOTEREJ	109
#define HOOK_RFC_CASECMP	110

/* these are FIXED once they are in a release they STAY
 * well, unless im feeling grumpy ;) */
#define MODCALL_START  0
#define MODCALL_CLOSE  1
#define MODCALL_EXPMEM 2
#define MODCALL_REPORT 3

/* share */
#define SHARE_FINISH       4
#define SHARE_DUMP_RESYNC  5

/* channels */
#define CHANNEL_CLEAR     15

/* server */
#define SERVER_BOTNAME       4
#define SERVER_BOTUSERHOST   5

/* irc */
#define IRC_RECHECK_CHANNEL 15

/* notes */
#define NOTES_CMD_NOTE       4

/* console */
#define CONSOLE_DOSTORE      4

struct hook_entry {
  struct hook_entry *next;
  int (*func) ();
} *hook_list[REAL_HOOKS];

#define call_hook(x) { struct hook_entry *p; \
for (p = hook_list[x]; p; p = p->next) p->func(); }

#ifdef HPUX_HACKS
#include <dl.h>
#endif
