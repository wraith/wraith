#ifndef _DCCUTIL_H
#define _DCCUTIL_H

#include "common.h"
#include "dcc.h"

#define dprintf dprintf_eggdrop

/* Public structure for the listening port map */
struct portmap {
  int realport;
  int mappedto;
  struct portmap *next;
};

/* Fake idx's for dprintf - these should be ridiculously large +ve nums
 */
#define DP_STDOUT       0x7FF1
#define DP_LOG          0x7FF2
#define DP_SERVER       0x7FF3
#define DP_HELP         0x7FF4
#define DP_STDERR       0x7FF5
#define DP_MODE         0x7FF6
#define DP_MODE_NEXT    0x7FF7
#define DP_SERVER_NEXT  0x7FF8
#define DP_HELP_NEXT    0x7FF9


#ifndef MAKING_MODS
void dprintf (int, ...);
void chatout (char *, ...);
void chanout_but (int, ...);
void dcc_chatter(int);
void lostdcc(int);
void makepass(char *);
void tell_dcc(int);
void not_away(int);
void set_away(int, char *);
void dcc_remove_lost(void);
void flush_lines(int, struct chat_info *);
struct dcc_t *find_idx(int);
int new_dcc(struct dcc_table *, int);
void del_dcc(int);
char *add_cr(char *);
void changeover_dcc(int, struct dcc_table *, int);
void do_boot(int, char *, char *);
int detect_dcc_flood(time_t *, struct chat_info *, int);

extern int		max_dcc, connect_timeout;
#endif /* !MAKING_MODS */

#endif /* !_DCCUTIL_H */
