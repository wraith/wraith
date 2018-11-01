#ifndef _MISC_H
#define _MISC_H

#include "common.h"


/*
 * Set the following to the timestamp for the logfile entries.
 * Popular times might be "[%H:%M]" (hour, min), or "[%H:%M:%S]" (hour, min, sec)
 * Read `man strftime' for more formatting options.  Keep it below 32 chars.
 */
#define LOG_TS "[%H:%M:%S]"

// Define this to be the length of the entire timestamp after replacing vars
#define LOG_TS_LEN 10


void restart(int);
int coloridx(int) __attribute__((pure));
const char *color(int, int, int) __attribute__((pure));
void shuffle(char *, char *, size_t);
void shuffleArray(char **, size_t);
void showhelp(int, struct flag_record *, const char *);
char *replace(const char *, const char *, const char *);
char *replace_vars(char*);
int goodpass(const char *, int, char *);
int bot_aggressive_to(struct userrec *);
void readsocks(const char *);
int updatebin(int, char *, int);
size_t my_strcpy(char *, const char *);
void maskaddr(const char *, char *, int);
#define maskhost(a,b) maskaddr((a),(b),3)
#define maskban(a,b)  maskaddr((a),(b),3)
void splitc(char *, char *, char);
void splitcn(char *, char *, char, size_t);
int remove_crlf(char *);
int remove_crlf_r(char *);
char *newsplit(char **, char delim = ' ', bool trim = 1);
char *splitnick(char **);
void stridx(char *, char *, int);
void daysago(time_t, time_t, char *, size_t);
void days(time_t, time_t, char *, size_t);
void daysdur(time_t, time_t, char *, size_t, bool = true);
void show_motd(int);
void show_channels(int, char *);
void show_banner(int);
void make_rand_str(char *, size_t, bool = 1);
char *str_escape(const char *, const char, const char);
char *strchr_unescape(char *, const char, const char);
void str_unescape(char *, const char);
int str_isdigit(const char *) __attribute__((pure));
void kill_bot(char *, char *);
char *strtolower(char *);
char *strtoupper(char *);
char *step_thru_file(FILE *);
char *trim(char *);
int skipline(char *, int *);


extern int		server_lag;
extern bool		use_invites, use_exempts;

#endif /* !_MISC_H_ */


