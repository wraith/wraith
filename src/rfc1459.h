#ifndef _RFC1459_H
#define _RFC1459_H

#ifndef MAKING_MODS
extern int (*rfc_casecmp) (const char *, const char *);
extern int (*rfc_ncasecmp) (const char *, const char *, int);
extern int (*rfc_toupper) (int);
extern int (*rfc_tolower) (int);

int _rfc_casecmp(const char *, const char *);
int _rfc_ncasecmp(const char *, const char *, int);
int _rfc_toupper(int);
int _rfc_tolower(int);

#endif /* !MAKING_MODS */


#endif /* !_RFC1459_H */
