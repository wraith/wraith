#ifndef _AUTH_H
#define _AUTH_H

#ifndef MAKING_MODS
int new_auth();
int findauth(char *);
void removeauth(int);
char *makehash(struct userrec *, char *);
#endif /* !MAKING_MODS */

#endif /* !_AUTH_H */
