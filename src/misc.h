#ifndef _MISC_H
#define _MISC_H

#include "common.h"


/*
 * Set the following to the timestamp for the logfile entries.
 * Popular times might be "[%H:%M]" (hour, min), or "[%H:%M:%S]" (hour, min, sec)
 * Read `man strftime' for more formatting options.  Keep it below 32 chars.
 */
#define LOG_TS "[%H:%M]"


char *wbanner(void);
void restart(int) __attribute__((noreturn));
int coloridx(int);
const char *color(int, int, int);
void shuffle(char *, char *);
void shuffleArray(char **, size_t);
void showhelp(int, struct flag_record *, char *);
char *replace(const char *, const char *, const char *);
int goodpass(char *, int, char *);
int bot_aggressive_to(struct userrec *);
int updatebin(int, char *, int);
size_t my_strcpy(char *, char *);
void maskhost(const char *, char *);
char *stristr(char *, char *);
void splitc(char *, char *, char);
void splitcn(char *, char *, char, size_t);
int remove_crlf(char *);
int remove_crlf_r(char *);
char *newsplit(char **);
char *splitnick(char **);
void stridx(char *, char *, int);
void daysago(time_t, time_t, char *);
void days(time_t, time_t, char *);
void daysdur(time_t, time_t, char *);
void show_motd(int);
void show_channels(int, char *);
void show_banner(int);
void make_rand_str(char *, size_t);
char *str_escape(const char *, const char, const char);
char *strchr_unescape(char *, const char, register const char);
void str_unescape(char *, register const char);
int str_isdigit(const char *);
void kill_bot(char *, char *);
char *strtolower(char *);
char *strtoupper(char *);
char *step_thru_file(FILE *);
char *trim(char *);
int skipline(char *, int *);
bool check_master_hash(const char *, const char *);


extern int		server_lag;
extern bool		use_invites, use_exempts;

#endif /* !_MISC_H_ */


