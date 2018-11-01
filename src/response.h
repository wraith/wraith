#ifndef _RESPONSE_H
#define _RESPONSE_H

#define RES_BANNED	"banned"
#define RES_KICKBAN	"kickban"
#define RES_MASSDEOP	"massdeop"
#define RES_BADOP	"badop"
#define RES_BADOPPED	"badopped"
#define RES_BITCHOP	"bitchop"
#define RES_BITCHOPPED	"bitchopped"
#define RES_MANUALOP	"manualop"
#define RES_MANUALOPPED	"manualopped"
#define RES_CLOSED	"closed"
#define RES_FLOOD	"flood"
#define RES_NICKFLOOD	"nickflood"
#define RES_KICKFLOOD	"kickflood"
#define RES_REVENGE	"revenge"
#define RES_USERNAME	"username"
#define RES_PASSWORD	"password"
#define RES_BADUSERPASS	"baduserpass"
#define RES_MIRCVER	"mIRCver"
#define RES_MIRCSCRIPT	"mIRCscript"
#define RES_OTHERSCRIPT	"otherscript"

typedef const char* response_t;

void init_responses();
const char *response(response_t) __attribute__((pure));

inline const char *
__attribute__((pure))
r_banned(struct chanset_t *chan)
{
  return response(RES_BANNED);
}

#endif /* !_RESPONSE_H */
