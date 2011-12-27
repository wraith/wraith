#ifndef _RFC1459_H
#define _RFC1459_H

int _rfc_casecmp(const char *, const char *);
int _rfc_ncasecmp(const char *, const char *, size_t);
int _rfc_toupper(int);

extern int (*rfc_casecmp) (const char *, const char *);
extern int (*rfc_ncasecmp) (const char *, const char *, size_t);
extern int (*rfc_toupper) (int);

#endif /* !_RFC1459_H */
