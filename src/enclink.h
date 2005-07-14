/* enclink.h
 *
 */

#ifndef _ENCLINK_H
#define _ENCLINK_H

#include <sys/types.h>

/* must leave old ones in here */
enum {
        LINK_GHOST = 0,	/* attic */
	LINK_GHOSTNAT,
        LINK_GHOSTSHA1, /* attic */
        LINK_GHOSTMD5, /* attic */
        LINK_CLEARTEXT,
	LINK_GHOSTCASE
};
enum direction_t {
        FROM,
        TO
};

struct enc_link {
  const char *name;
  int type;
  void (*link) (int, direction_t);
  char *(*write) (int, char *, size_t *);
  int (*read) (int, char *, size_t *);
  void (*parse) (int, int, char *);
};


extern struct enc_link enclink[];


extern int link_find_by_type(int);

extern void link_link(int, int, int, direction_t);
extern char *link_write(int, char *, size_t *);
extern int link_read(int, char *, size_t *);
extern void link_hash(int, char *);
extern void link_send(int, const char *, ...) __attribute__((format(printf, 2, 3)));
extern void link_done(int);
extern void link_parse(int, char *);

#endif /* !_ENCLINK_H */
