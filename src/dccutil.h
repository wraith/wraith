#ifndef _DCCUTIL_H
#define _DCCUTIL_H

#include "common.h"
#include "dcc.h"

/* Public structure for the listening port map */
struct portmap {
  in_port_t realport;
  in_port_t mappedto;
  struct portmap *next;
};

namespace bd {
  class Stream;
  class String;
}

#define dprintf dprintf_eggdrop

/* Fake idx's for dprintf - these should be ridiculously large +ve nums
 */
#define DP_STDOUT       0x7FF1
//#define DP_LOG          0x7FF2
#define DP_DEBUG	0x7FF2
#define DP_SERVER       0x7FF3
#define DP_HELP         0x7FF4
#define DP_STDERR       0x7FF5
#define DP_MODE         0x7FF6
#define DP_MODE_NEXT    0x7FF7
#define DP_SERVER_NEXT  0x7FF8
#define DP_HELP_NEXT    0x7FF9
#define DP_DUMP		0x8000
#define DP_CACHE	0x8001
#define DP_PLAY		0x8002



void init_dcc(void);
void dumplots(int, const char *, bd::String);
void rdprintf(const char*, int, const char *, ...) __attribute__((format(printf, 3, 4)));
void dprintf(int, const char *, ...) __attribute__((format(printf, 2, 3)));
void dprintf_real(int, char*, size_t, size_t, const char* = NULL);
void chatout(const char *, ...) __attribute__((format(printf, 1, 2)));
void chanout_but(int, int, const char *, ...) __attribute__((format(printf, 3, 4)));
void dcc_chatter(int);
void lostdcc(int);
void makepass(char *);
void tell_dcc(int);
void not_away(int);
void set_away(int, char *);
void dcc_remove_lost(void);
void flush_lines(int, struct chat_info *);
int new_dcc(struct dcc_table *, int);
void del_dcc(int);
char *add_cr(char *);
void changeover_dcc(int, struct dcc_table *, int);
void do_boot(int, const char *, const char *);
int detect_dcc_flood(time_t *, struct chat_info *, int);
void identd_open(const char * = NULL, const char * = NULL, int identd = 1);
void identd_close();
int listen_all(in_port_t, bool, bool);
static inline bool __attribute__((pure))
valid_idx(int idx)
{
  if ((idx == -1) || (idx >= dcc_total) || (!dcc[idx].type))
    return false;
  return true;
}

int dcc_read(bd::Stream&);
void dcc_write(bd::Stream&, int);
int check_cmd_pass(const char *, char *);
int has_cmd_pass(const char *) __attribute__((pure));
void set_cmd_pass(char *, int);
void cmdpass_free(struct cmd_pass *);

extern int		max_dcc;
extern interval_t		connect_timeout;

#endif /* !_DCCUTIL_H */
