/*
 * bg.h
 *
 */

#ifndef _EGG_BG_H
#define _EGG_BG_H

typedef enum {
	BG_QUIT = 1,
	BG_ABORT
} bg_quit_t;

void bg_prepare_split(void);
void bg_send_quit(bg_quit_t q);
void bg_do_split(void);

#endif			/* _EGG_BG_H */
