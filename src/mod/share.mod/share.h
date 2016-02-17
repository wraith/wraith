/*
 * share.h -- part of share.mod
 *
 */

#ifndef _EGG_MOD_SHARE_SHARE_H
#define _EGG_MOD_SHARE_SHARE_H

#define	UFF_OVERRIDE	BIT0	/* Override existing bot entries    */
#define UFF_INVITE	BIT1	/* Send invites in user file	    */
#define UFF_EXEMPT	BIT2	/* Send exempts in user file	    */
#define UFF_CHDEFAULT	BIT3

#include "src/users.h"

void sharein(int, char *);
void shareout(const char *, ...) __attribute__((format(printf, 1, 2)));
void shareout_prot(struct userrec *, const char *, ...) __attribute__((format(printf, 2, 3)));
void shareout_hub(const char *, ...) __attribute__((format(printf, 1, 2)));
void finish_share(int);
void dump_resync(int);
void share_report(int, int);
void hook_read_userfile();

#endif				/* _EGG_MOD_SHARE_SHARE_H */
