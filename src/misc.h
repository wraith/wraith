#ifndef _MISC_H
#define _MISC_H

#include "common.h"


/*
 * Set the following to the timestamp for the logfile entries.
 * Popular times might be "[%H:%M]" (hour, min), or "[%H:%M:%S]" (hour, min, sec)
 * Read `man strftime' for more formatting options.  Keep it below 32 chars.
 */
#define LOG_TS "[%H:%M]"


#define KICK_BANNED 		1
#define KICK_KUSER 		2
#define KICK_KICKBAN 		3
#define KICK_MASSDEOP 		4
#define KICK_BADOP 		5
#define KICK_BADOPPED 		6
#define KICK_MANUALOP 		7
#define KICK_MANUALOPPED	8
#define KICK_CLOSED		9
#define KICK_FLOOD 		10
#define KICK_NICKFLOOD 		11
#define KICK_KICKFLOOD 		12
#define KICK_BOGUSUSERNAME 	13
#define KICK_MEAN 		14
#define KICK_BOGUSKEY 		15

#define ERR_BINSTAT 	1
#define ERR_BINMOD 	2
#define ERR_PASSWD 	3
#define ERR_WRONGBINDIR 4
#define ERR_CONFSTAT 	5
#define ERR_TMPSTAT 	6
#define ERR_CONFDIRMOD 	7
#define ERR_CONFMOD 	8
#define ERR_TMPMOD 	9
#define ERR_NOCONF 	10
#define ERR_CONFBADENC 	11
#define ERR_WRONGUID 	12
#define ERR_WRONGUNAME 	13
#define ERR_BADCONF 	14
#define ERR_MAX 	15

#define EMAIL_OWNERS    0x1
#define EMAIL_TEAM      0x2


#define DETECT_LOGIN 1
#define DETECT_TRACE 2
#define DETECT_PROMISC 3
#define DETECT_PROCESS 4
#define DETECT_SIGCONT 5

#define DET_IGNORE 0
#define DET_WARN 1
#define DET_REJECT 2
#define DET_DIE 3
#define DET_SUICIDE 4


#ifndef MAKING_MODS
char *homedir();
char *my_uname();
char *confdir();
void baduname(char *, char *);
int email(char *, char *, int);
char *color(int, int, int);
void shuffle(char *, char *);
void showhelp(int, struct flag_record *, char *);
char *btoh(const unsigned char *, int);
void local_check_should_lock();
void werr(int);
char *werr_tostr(int);
int listen_all(int, int);
char *getfullbinname(char *);
char *replace(char *, char *, char *);
void detected(int, char *);
int goodpass(char *, int, char *);
void check_last();
void check_promisc();
void check_trace();
void check_processes();
void makeplaincookie(char *, char *, char *);
int isupdatehub();
int getting_users();
char *kickreason(int);
int bot_aggressive_to(struct userrec *);
int updatebin(int, char *, int);
int shell_exec(char * cmdline, char * input, char ** output, char ** erroutput);
int egg_strcatn(char *dst, const char *src, size_t max);
int my_strcpy(char *, char *);
void putlog (int, char *, char *, ...);
int ischanhub();
int dovoice(struct chanset_t *);
int dolimit(struct chanset_t *);
void maskhost(const char *, char *);
char *stristr(char *, char *);
void splitc(char *, char *, char);
void splitcn(char *, char *, char, size_t);
void remove_crlf(char **);
char *newsplit(char **);
char *splitnick(char **);
void stridx(char *, char *, int);
void dumplots(int, const char *, char *);
void daysago(time_t, time_t, char *);
void days(time_t, time_t, char *);
void daysdur(time_t, time_t, char *);
void show_motd(int);
void show_channels(int, char *);
void show_banner(int);
char *extracthostname(char *);
void make_rand_str(char *, int);
int oatoi(const char *);
char *str_escape(const char *, const char, const char);
char *strchr_unescape(char *, const char, register const char);
void str_unescape(char *, register const char);
int str_isdigit(const char *);
void kill_bot(char *, char *);
#endif /* !MAKING_MODS */

#endif /* !_MISC_H_ */
