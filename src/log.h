#ifndef _EGG_LOG_H
#define _EGG_LOG_H

#define LCAT_BOT        "bot"
#define LINFO_BOT       "Bot linking/sharing/unlinking", 0, 0, 0, USER_MASTER

#define LCAT_BOTMODE    "botmode"
#define LINFO_BOTMODE   "KICKs and MODEs by bots", 0, 0, 0, USER_OP

#define LCAT_CHANNEL    "channel"
#define LINFO_CHANNEL   "Joins, parts, quits, misc channel info", 0, 0, 0, USER_OWNER

#define LCAT_COMMAND    "command"
#define LINFO_COMMAND   "DCC Commands", 0, 1, 1, USER_MASTER

#define LCAT_CONN       "conn"
#define LINFO_CONN      "Anything related to connections/relays", 1, 1, 1, USER_OP

#define LCAT_DEBUG      "debug"
#define LINFO_DEBUG     "Debug information", 0, 0, 0, USER_OWNER

#define LCAT_ERROR      "error"
#define LINFO_ERROR     "Fatal and non-fatal errors", 1, 1, 1, USER_MASTER

#define LCAT_GETIN      "getin"
#define LINFO_GETIN     "Bots requesting/giving ops, keys, invites, unbans & limitraises", 0, 0, 0, USER_OP

#define LCAT_INFO       "info"
#define LINFO_INFO      "Miscellaneous information", 0, 0, 0, USER_PARTY

#define LCAT_MESSAGE    "message"
#define LINFO_MESSAGE   "Msgs, notices & ctcps to the bot", 0, 1, 1, USER_OP

#define LCAT_PUBLIC     "public"
#define LINFO_PUBLIC    "Public chat in console channel", 0, 0, 0, USER_OWNER

#define LCAT_ULIST      "userlist"
#define LINFO_ULIST     "All changes to userlist", 1, 1, 1, USER_MASTER

#define LCAT_USERMODE   "usermode"
#define LINFO_USERMODE  "KICKs and MODEs by nonbots", 0, 1, 1, USER_OP

#define LCAT_WARNING    "warning"
#define LINFO_WARNING   "Warnings on unexpected behaviour/security related", 1, 1, 1, USER_MASTER

#ifdef DEBUG_MEM
#define LCAT_RAW        "raw"
#define LINFO_RAW       "Raw data from server", 0, 0, 0, USER_OWNER

#define LCAT_RAWOUT     "rawout"
#define LINFO_RAWOUT    "Raw data to server", 0, 0, 0, USER_OWNER
#endif

#define LCAT_MISC "old_misc"
#define LCAT_BOTS "old_bot"
#define LCAT_MODE "old_mode"

struct logcategory {
  struct logcategory *next;
  char *name;
  char *desc;
  int logtochan;
  int logtofile;
  int broadcast;
  int flags;
};

#endif /* _EGG_LOG_H */
