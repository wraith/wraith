/* src/mod/static.h -- header file for static compiles.
 *
 */

#ifndef _EGG_MOD_STATIC_H
#define _EGG_MOD_STATIC_H

void dns_init();
void console_init();
void ctcp_init();
void update_init();
void notes_init();
#ifdef LEAF
void server_init();
void irc_init();
#endif /* LEAF */
void channels_init();

void compress_init();
void share_init();
void transfer_init();

#endif /* _EGG_MOD_STATIC_H */
