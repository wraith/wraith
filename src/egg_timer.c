#include <stdio.h> /* For NULL */
#include <sys/time.h> /* For gettimeofday() */
#include "common.h"

#include "egg_timer.h"

/* Internal use only. */
typedef struct egg_timer_b {
	struct egg_timer_b *next;
	int id;
	Function callback;
	void *client_data;
	egg_timeval_t howlong;
	egg_timeval_t trigger_time;
	int flags;
} egg_timer_t;

/* We keep a sorted list of active timers. */
static egg_timer_t *timer_list_head = NULL;
static unsigned int timer_next_id = 1;

/* Based on TclpGetTime from Tcl 8.3.3 */
int timer_get_time(egg_timeval_t *curtime)
{
	struct timeval tv;
	struct timezone tz;

	(void) gettimeofday(&tv, &tz);
	curtime->sec = tv.tv_sec;
	curtime->usec = tv.tv_usec;
	return(0);
}

int timer_create_complex(egg_timeval_t *howlong, Function callback, void *client_data, int flags)
{
	egg_timer_t *timer = NULL, *prev = NULL;
	egg_timeval_t trigger_time;

	timer_get_time(&trigger_time);
	trigger_time.sec += howlong->sec;
	trigger_time.usec += howlong->usec;

	/* Find out where this should go in the list. */
	prev = NULL;
	for (timer = timer_list_head; timer; timer = timer->next) {
		if (trigger_time.sec < timer->trigger_time.sec) break;
		if (trigger_time.sec == timer->trigger_time.sec && trigger_time.usec < timer->trigger_time.usec) break;
		prev = timer;
	}

	/* Fill out a new timer. */
	timer = (egg_timer_t *) calloc(1, sizeof(*timer));
	timer->callback = callback;
	timer->client_data = client_data;
	timer->flags = flags;
	egg_memcpy(&timer->howlong, howlong, sizeof(*howlong));
	egg_memcpy(&timer->trigger_time, &trigger_time, sizeof(trigger_time));
	timer->id = timer_next_id++;

	/* Insert into timer list. */
	if (prev) {
		timer->next = prev->next;
		prev->next = timer;
	}
	else {
		timer->next = timer_list_head;
		timer_list_head = timer;
	}

	if (timer_next_id == 0) timer_next_id++;

	return(timer->id);
}

/* Destroy a timer, given an id. */
int timer_destroy(int timer_id)
{
	egg_timer_t *prev = NULL, *timer = NULL;

	prev = NULL;
	for (timer = timer_list_head; timer; timer = timer->next) {
		if (timer->id == timer_id) break;
	}

	if (!timer) return(1); /* Not found! */

	/* Unlink it. */
	if (prev) prev->next = timer->next;
	else timer_list_head = timer->next;

	free(timer);
	return(0);
}

int timer_destroy_all()
{
	egg_timer_t *timer = NULL;

	for (timer = timer_list_head; timer; timer = timer->next) {
		free(timer);
	}
	timer_list_head = NULL;
	return(0);
}

int timer_get_shortest(egg_timeval_t *howlong)
{
	egg_timeval_t curtime;
	egg_timer_t *timer = timer_list_head;

	/* No timers? Boo. */
	if (!timer) return(1);

	timer_get_time(&curtime);

	if (timer->trigger_time.sec <= curtime.sec) howlong->sec = 0;
	else howlong->sec = timer->trigger_time.sec - curtime.sec;

	if (timer->trigger_time.usec <= curtime.usec) howlong->usec = 0;
	else howlong->usec = timer->trigger_time.usec - curtime.usec;

	return(0);
}

int timer_run()
{
	egg_timeval_t curtime;
	egg_timer_t *timer = NULL;
	Function callback;
	void *client_data = NULL;

	while ((timer = timer_list_head)) {
		timer_get_time(&curtime);
		if (timer->trigger_time.sec > curtime.sec || (timer->trigger_time.sec == curtime.sec && timer->trigger_time.usec > curtime.usec)) break;

		timer_list_head = timer_list_head->next;

		callback = timer->callback;
		client_data = timer->client_data;

		if (timer->flags & TIMER_REPEAT) {
			egg_timer_t *prev, *tptr;

			/* Update timer. */
			timer->trigger_time.sec += timer->howlong.sec;
			timer->trigger_time.usec += timer->howlong.usec;

			prev = NULL;
			for (tptr = timer_list_head; tptr; tptr = tptr->next) {
				if (tptr->trigger_time.sec > timer->trigger_time.sec || (tptr->trigger_time.sec == timer->trigger_time.sec && tptr->trigger_time.usec > timer->trigger_time.usec)) break;
				prev = tptr;
			}
			if (prev) {
				timer->next = prev->next;
				prev->next = timer;
			}
			else {
				timer->next = timer_list_head;
				timer_list_head = timer;
			}
		}
		else {
			free(timer);
		}
		callback(client_data);
	}
	return(0);
}
