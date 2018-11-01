#ifndef _EGG_TIMER_H_
#define _EGG_TIMER_H_

#include "types.h"

typedef struct egg_timeval_b {
	long sec;
	long usec;
} egg_timeval_t;

#define TIMER_REPEAT 1

/* Create a simple timer with no client data and no flags. */
#define timer_create(howlong,name,callback) timer_create_complex(howlong, name, callback, NULL, 0)

/* Create a simple timer with no client data, but it repeats. */
#define timer_create_repeater(howlong,name,callback) timer_create_complex(howlong, name, callback, NULL, TIMER_REPEAT)

void timer_get_now(egg_timeval_t *_now);
int timer_get_now_sec(int *sec);
void timer_update_now(egg_timeval_t *_now);
int timer_diff(egg_timeval_t *from_time, egg_timeval_t *to_time, egg_timeval_t *diff);
long timeval_diff(const egg_timeval_t *tv1, const egg_timeval_t *tv2)
  __attribute__((pure));
int timer_create_secs(int, const char *, Function);
int timer_create_complex(egg_timeval_t *howlong, const char *name, Function callback, void *client_data, int flags);
int timer_destroy(int timer_id);
#ifdef not_used
int timer_destroy_all();
#endif
int timer_get_shortest(egg_timeval_t *howlong);
void timer_run();
int timer_list(int **ids);
int timer_info(int id, char **name, egg_timeval_t *initial_len, egg_timeval_t *trigger_time, int *called);
#endif /* _EGG_TIMER_H_ */
