#ifndef _MATCH_H
#define _MATCH_H

int _wild_match(register unsigned char *, register unsigned char *);
int _wild_match_per(register unsigned char *, register unsigned char *);

#define wild_match(a,b) _wild_match((unsigned char *)(a),(unsigned char *)(b))
#define wild_match_per(a,b) _wild_match_per((unsigned char *)(a),(unsigned char *)(b))

#endif /* !_MATCH_H */
