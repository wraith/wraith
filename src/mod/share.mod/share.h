/*
 * share.h -- part of share.mod
 *
 */

#ifndef _EGG_MOD_SHARE_SHARE_H
#define _EGG_MOD_SHARE_SHARE_H

#define	UFF_OVERRIDE	BIT0	/* Override existing bot entries    */
#define UFF_INVITE	BIT1	/* Send invites in user file	    */
#define UFF_EXEMPT	BIT2	/* Send exempts in user file	    */

void sharein(int, char *);
void shareout(struct chanset_t *, ...);
void finish_share(int);
void dump_resync(int);
void share_report(int, int);
#ifdef HUB
void hook_read_userfile();
#endif /* HUB */

#endif				/* _EGG_MOD_SHARE_SHARE_H */
