#ifndef _TRAFFIC_H_
#define _TRAFFIC_H_

#include <sys/types.h>

typedef struct {
	struct {
		size_t irc;
		size_t bn;
		size_t dcc;
		size_t filesys;
		size_t trans;
		size_t unknown;
	} in_total, in_today, out_total, out_today;
} egg_traffic_t;

#endif /* _TRAFFIC_H_ */
