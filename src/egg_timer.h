#ifndef _EGG_TIMER_H_
#define _EGG_TIMER_H_

#include "types.h"

typedef struct egg_timeval_b {
	int sec;
	int usec;
} egg_timeval_t;

#define TIMER_REPEAT 1

/* Create a simple timer with no client data and no flags. */
#define timer_create(howlong,callback) timer_create_complex(howlong, callback, NULL, 0)

/* Create a simple timer with no client data, but it repeats. */
#define timer_create_repeater(howlong,callback) timer_create_complex(howlong, callback, NULL, TIMER_REPEAT)

int timer_get_time(egg_timeval_t *curtime);
int timer_create_complex(egg_timeval_t *howlong, Function callback, void *client_data, int flags);
int timer_destroy(int timer_id);
int timer_destroy_all();
int timer_get_shortest(egg_timeval_t *howlong);
int timer_run();

#endif /* _EGG_TIMER_H_ */
