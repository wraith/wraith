/* src/mod/static.h -- header file for static compiles.
 *
 */

#ifndef _EGG_MOD_STATIC_H
#define _EGG_MOD_STATIC_H

char *channels_start();
char *compress_start();
char *dns_start();
#ifdef LEAF
char *irc_start();
#endif
char *notes_start();
#ifdef LEAF
char *server_start();
#endif
char *share_start();
char *transfer_start();

void console_init();
void ctcp_init();
void update_init();

static void link_statics()
{
  check_static("channels", channels_start);
  check_static("compress", compress_start);
  check_static("dns", dns_start);
#ifdef LEAF
  check_static("irc", irc_start);
#endif
  check_static("notes", notes_start);
#ifdef LEAF
  check_static("server", server_start);
#endif
  check_static("share", share_start);
  check_static("transfer", transfer_start);
}

#endif /* _EGG_MOD_STATIC_H */
