/*
 * update.h -- part of update.mod
 *
 */

#ifndef _EGG_MOD_update_update_H
#define _EGG_MOD_update_update_H

/* Currently reserved flags for other modules:
 *      UFF_COMPRESS    0x000008	   Compress the user file
 *      UFF_ENCRYPT	0x000010	   Encrypt the user file
 */

/* Currently used priorities:
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

#ifndef MAKING_update
/* 4 - 7 */
#define finish_update ((void (*) (int))update_funcs[4])
//#define dump_resync ((void (*) (int))update_funcs[5])
//#define uff_addtable ((void (*) (uff_table_t *))update_funcs[6])
//#define uff_deltable ((void (*) (uff_table_t *))update_funcs[7])
/* 8 - 11 */
#ifdef HUB
#define bupdating (*(int*)update_funcs[8])
#endif /* HUB */
#endif				/* !MAKING_update */

#endif				/* _EGG_MOD_update_update_H */
