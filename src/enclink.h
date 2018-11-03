/* enclink.h
 *
 */

#ifndef _ENCLINK_H
#define _ENCLINK_H

#include <sys/types.h>

/* must leave old ones in here */
enum {
        LINK_GHOST = 0,	/* attic */
	LINK_GHOSTNAT, /* attic */
        LINK_GHOSTSHA1, /* attic */
        LINK_GHOSTMD5, /* attic */
        LINK_CLEARTEXT,
	LINK_GHOSTCASE, /* attic */
	LINK_GHOSTCASE2, /* attic */
	LINK_GHOSTCASE3
};
enum direction_t {
        FROM,
        TO
};

struct enc_link {
  const char *name;
  int type;
  void (*link) (int, direction_t);
  const char *(*write) (int, const char *, size_t *);
  int (*read) (int, char *);
  void (*parse) (int, int, char *);
};

struct enc_link_dcc {
  struct enc_link *method;
  int method_number;
};

extern struct enc_link enclink[];


extern int link_find_by_type(int) __attribute__((pure));

extern void link_link(int, int, int, direction_t);
extern const char *link_write(int, const char *, size_t *);
extern int link_read(int, char *);
extern void link_hash(int, char *);
extern void link_send(int, const char *, ...) __attribute__((format(printf, 2, 3)));
extern void link_done(int);
extern void link_parse(int, char *);
extern void link_get_method(int);
extern void link_challenge_to(int idx, char *buf);

#endif /* !_ENCLINK_H */
