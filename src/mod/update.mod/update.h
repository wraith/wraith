/*
 * update.h -- part of update.mod
 *
 */

#ifndef _EGG_MOD_update_update_H
#define _EGG_MOD_update_update_H

extern int bupdating;

void finish_update(int);
void update_report(int, int);
void updatein(int, char *);

#endif				/* _EGG_MOD_update_update_H */
