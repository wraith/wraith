/*
 * share.h -- part of share.mod
 *
 */

#ifndef _EGG_MOD_SHARE_SHARE_H
#define _EGG_MOD_SHARE_SHARE_H

#define	UFF_OVERRIDE	BIT0	/* Override existing bot entries    */
#define UFF_INVITE	BIT1	/* Send invites in user file	    */
#define UFF_EXEMPT	BIT2	/* Send exempts in user file	    */
#define UFF_CHANS	BIT3	/* Send channels in user file */
#define UFF_TCL		BIT4	/* Send tcl file with userfile */

/* Currently reserved flags for other modules:
 *      UFF_COMPRESS    0x000008	   Compress the user file
 *      UFF_ENCRYPT	0x000010	   Encrypt the user file
 */

/* Currently used priorities:
 *        0		UFF_OVERRIDE
 *        0		UFF_INVITE
 *        0		UFF_EXEMPT
 *       90		UFF_ENCRYPT
 *      100             UFF_COMPRESS
 */

typedef struct {
  char	 *feature;		/* Name of the feature			*/
  int	  flag;			/* Flag representing the feature	*/
  int	(*ask_func)(int);	/* Pointer to the function that tells
				   us wether the feature should be
				   considered as on.			*/
  int	  priority;		/* Priority with which this entry gets
				   called.				*/
  int	(*snd)(int, char *);	/* Called before sending. Handled
				   according to `priority'.		*/
  int	(*rcv)(int, char *);	/* Called on receive. Handled according
				   to `priority'.			*/
} uff_table_t;

void sharein(int, char *);
void shareout(struct chanset_t *, ...);
void finish_share(int);
void dump_resync(int);
void uff_addtable(uff_table_t *);
void share_report(int, int);
#ifdef HUB
void hook_read_userfile();
#endif /* HUB */

#endif				/* _EGG_MOD_SHARE_SHARE_H */
