#include <stdio.h> /* For NULL */
#include <sys/time.h> /* For gettimeofday() */
#include "common.h"

#include "egg_timer.h"

/* From main.c */
static egg_timeval_t now;

/* Internal use only. */
typedef struct egg_timer_b {
	struct egg_timer_b *next;
	int id;
	char *name;
	Function callback;
	void *client_data;
	egg_timeval_t howlong;
	egg_timeval_t trigger_time;
	int flags;
	int called;
} egg_timer_t;

/* We keep a sorted list of active timers. */
static egg_timer_t *timer_list_head = NULL;
static int timer_next_id = 1;

/* Based on TclpGetTime from Tcl 8.3.3 */
int timer_get_time(egg_timeval_t *curtime)
{
	struct timeval tv;

	(void) gettimeofday(&tv, NULL);
	curtime->sec = tv.tv_sec;
	curtime->usec = tv.tv_usec;
	return(0);
}

int timer_update_now(egg_timeval_t *_now)
{
	timer_get_time(&now);
	if (_now) {
		_now->sec = now.sec;
		_now->usec = now.usec;
	}
	return(now.sec);
}


void timer_get_now(egg_timeval_t *_now)
{
	_now->sec = now.sec;
	_now->usec = now.usec;
}

int timer_get_now_sec(int *sec)
{
	if (sec) *sec = now.sec;
	return(now.sec);
}


/* Find difference between two timers. */
int timer_diff(egg_timeval_t *from_time, egg_timeval_t *to_time, egg_timeval_t *diff)
{
	diff->sec = to_time->sec - from_time->sec;
	if (diff->sec < 0) {
		diff->sec = 0;
		diff->usec = 0;
		return(1);
	}

	diff->usec = to_time->usec - from_time->usec;

	if (diff->usec < 0) {
		if (diff->sec == 0) {
			diff->usec = 0;
			return(1);
		}
		diff->sec -= 1;
		diff->usec += 1000000;
	}

	return(0);
}

static int timer_add_to_list(egg_timer_t *timer)
{
	egg_timer_t *prev = NULL, *ptr = NULL;

	/* Find out where this should go in the list. */
	for (ptr = timer_list_head; ptr; ptr = ptr->next) {
		if (timer->trigger_time.sec < ptr->trigger_time.sec) break;
		if (timer->trigger_time.sec == ptr->trigger_time.sec && timer->trigger_time.usec < ptr->trigger_time.usec) break;
		prev = ptr;
	}

	/* Insert into timer list. */
	if (prev) {
		timer->next = prev->next;
		prev->next = timer;
	}
	else {
		timer->next = timer_list_head;
		timer_list_head = timer;
	}
	return(0);
}

int timer_create_secs(int secs, const char *name, Function callback)
{
	egg_timeval_t howlong;

	howlong.sec = secs;
	howlong.usec = 0;

	return timer_create_repeater(&howlong, name, (Function) callback);
}

int timer_create_complex(egg_timeval_t *howlong, const char *name, Function callback, void *client_data, int flags)
{
	egg_timer_t *timer = NULL;

	/* Fill out a new timer. */
	timer = (egg_timer_t *)calloc(1, sizeof(*timer));
	timer->id = timer_next_id++;
	if (name) timer->name = strdup(name);
	else timer->name = NULL;
	timer->callback = callback;
	timer->client_data = client_data;
	timer->flags = flags;
	timer->howlong.sec = howlong->sec;
	timer->howlong.usec = howlong->usec;
	timer->trigger_time.sec = now.sec + howlong->sec;
	timer->trigger_time.usec = now.usec + howlong->usec;
	timer->called = 0;

	timer_add_to_list(timer);

	return(timer->id);
}

/* Destroy a timer, given an id. */
int timer_destroy(int timer_id)
{
	egg_timer_t *prev = NULL, *timer = NULL;

	prev = NULL;
	for (timer = timer_list_head; timer; timer = timer->next) {
		if (timer->id == timer_id) break;
		prev = timer;
	}

	if (!timer) return(1); /* Not found! */

	/* Unlink it. */
	if (prev) prev->next = timer->next;
	else timer_list_head = timer->next;

	if (timer->name) free(timer->name);
	free(timer);
	return(0);
}

int timer_destroy_all()
{
	egg_timer_t *timer = NULL, *next = NULL;

	for (timer = timer_list_head; timer; timer = next) {
		next = timer->next;
	}
	timer_list_head = NULL;
	return(0);
}

int timer_get_shortest(egg_timeval_t *howlong)
{
	egg_timer_t *timer = timer_list_head;

	/* No timers? Boo. */
	if (!timer) return(1);

	timer_diff(&now, &timer->trigger_time, howlong);
	return(0);
}

int timer_run()
{
	egg_timer_t *timer = NULL;
	Function callback;
	void *client_data = NULL;

	while (timer_list_head) {
		timer = timer_list_head;

		if (timer->trigger_time.sec > now.sec || 
			(timer->trigger_time.sec == now.sec && timer->trigger_time.usec > now.usec)) break;

		timer_list_head = timer_list_head->next;

		callback = timer->callback;
		client_data = timer->client_data;

		if (timer->flags & TIMER_REPEAT) {
			/* Update timer. */
			timer->trigger_time.sec += timer->howlong.sec;
			timer->trigger_time.usec += timer->howlong.usec;

			if (timer->trigger_time.usec >= 1000000) {
				timer->trigger_time.usec -= 1000000;
				timer->trigger_time.sec += 1;
			}

			/* Add it back into the list. */
			timer_add_to_list(timer);
		}
		else {
			if (timer->name) free(timer->name);
			free(timer);
		}

		timer->called++;
		callback(client_data);
	}
	return(0);
}

int timer_list(int **ids)
{
	egg_timer_t *timer = NULL;
	int ntimers = 0;

	/* Count timers. */
	for (timer = timer_list_head; timer; timer = timer->next) ntimers++;

	/* Fill in array. */
	*ids = malloc(sizeof(int) * (ntimers+1));
	ntimers = 0;
	for (timer = timer_list_head; timer; timer = timer->next) {
		(*ids)[ntimers++] = timer->id;
	}
	return(ntimers);
}

int timer_info(int id, char **name, egg_timeval_t *initial_len, egg_timeval_t *trigger_time, int *called)
{
        egg_timer_t *timer = NULL;

        for (timer = timer_list_head; timer; timer = timer->next) {
                if (timer->id == id) break;
        }
        if (!timer) return(-1);
	if (name) *name = timer->name;
        if (initial_len) memcpy(initial_len, &timer->howlong, sizeof(*initial_len));
        if (trigger_time) memcpy(trigger_time, &timer->trigger_time, sizeof(*trigger_time));
	if (called) *called = timer->called;
        return(0);
}


