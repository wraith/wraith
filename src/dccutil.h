#ifndef _DCCUTIL_H
#define _DCCUTIL_H

#include "eggmain.h"

#ifdef HAVE_DPRINTF
#  define dprintf dprintf_eggdrop
#endif

#ifndef MAKING_MODS
void dprintf (int, ...);
void chatout (char *, ...);
extern void (*shareout) ();
extern void (*sharein) (int, char *);
extern void (*shareupdatein) (int, char *);
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
#endif /* !MAKING_MODS */

#endif /* !_DCCUTIL_H */
