#ifndef _TRAFFIC_H_
#define _TRAFFIC_H_

typedef struct {
	struct {
		unsigned long irc;
		unsigned long bn;
		unsigned long dcc;
		unsigned long filesys;
		unsigned long trans;
		unsigned long unknown;
	} in_total, in_today, out_total, out_today;
} egg_traffic_t;

#endif /* _TRAFFIC_H_ */
