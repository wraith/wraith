/* src/mod/static.h -- header file for static compiles.
 *
 */

#ifndef _EGG_MOD_STATIC_H
#define _EGG_MOD_STATIC_H

char *compress_start();
char *share_start();
char *transfer_start();

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

static void link_statics()
{
  check_static("compress", compress_start);
  check_static("share", share_start);
  check_static("transfer", transfer_start);
}

#endif /* _EGG_MOD_STATIC_H */
