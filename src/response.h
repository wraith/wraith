#ifndef _RESPONSE_H
#define _RESPONSE_H

typedef unsigned int response_t;

enum {
	RES_BANNED = 1,
	RES_KICKBAN,
	RES_MASSDEOP,
	RES_BADOP,
	RES_BADOPPED,
	RES_BITCHOP,
	RES_BITCHOPPED,
	RES_MANUALOP,
	RES_MANUALOPPED,
	RES_CLOSED,
	RES_FLOOD,
	RES_NICKFLOOD,
	RES_KICKFLOOD,
	RES_REVENGE,
	RES_USERNAME,
	RES_PASSWORD,
	RES_BADUSERPASS,
	RES_MIRCVER,
	RES_MIRCSCRIPT,
	RES_OTHERSCRIPT,
	RES_END
};

#define RES_TYPES 20

const char *response(response_t);
void init_responses();
const char *r_banned(struct chanset_t* chan);

#endif /* !_RESPONSE_H */
