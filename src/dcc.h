#ifndef _DCC_H
#define _DCC_H

#ifndef MAKING_MODS

extern struct dcc_table DCC_CHAT, DCC_BOT, DCC_LOST, DCC_BOT_NEW,
 DCC_RELAY, DCC_RELAYING, DCC_FORK_RELAY, DCC_PRE_RELAY, DCC_CHAT_PASS,
 DCC_FORK_BOT, DCC_SOCKET, DCC_TELNET_ID, DCC_TELNET_NEW, DCC_TELNET_PW,
 DCC_TELNET, DCC_IDENT, DCC_IDENTWAIT, DCC_DNSWAIT;

void failed_link(int);
void dupwait_notify(char *);
char *rand_dccresp();
#endif /* !MAKING_MODS */

#endif /* !_DCC_H */
