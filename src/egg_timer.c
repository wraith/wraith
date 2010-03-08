/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2008 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#include <stdio.h> /* For NULL */
#include <sys/time.h> /* For gettimeofday() */
#include "common.h"

#include "egg_timer.h"

typedef int (*TimerFunc) (void *);

/* From main.c */
static egg_timeval_t now;

/* Internal use only. */
typedef struct egg_timer_b {
	struct egg_timer_b *next;
	int id;
	char *name;
        TimerFunc callback;
	void *client_data;
	egg_timeval_t howlong;
	egg_timeval_t trigger_time;
	int flags;
	int called;
} egg_timer_t;

/* We keep a sorted list of active timers. */
static egg_timer_t *timer_repeat_head = NULL, *timer_once_head = NULL;
static int timer_next_id = 1;

/* Based on TclpGetTime from Tcl 8.3.3 */
static inline int timer_get_time(egg_timeval_t *curtime)
{
	static struct timeval tv;

	(void) gettimeofday(&tv, NULL);
	curtime->sec = tv.tv_sec;
	curtime->usec = tv.tv_usec;
	return(0);
}

void timer_update_now(egg_timeval_t *_now)
{
	timer_get_time(&now);
	if (_now) timer_get_now(_now);
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
		--(diff->sec);
		diff->usec += 1000000;
	}

	return(0);
}

/*
 * Return milliseconds difference between two timevals
 */
long timeval_diff(const egg_timeval_t *tv1, const egg_timeval_t *tv2)
{
	long secs = tv1->sec - tv2->sec, usecs = tv1->usec - tv2->usec;
	if (usecs < 0) {
		usecs += 1000000;
		--secs;
	}
	usecs = (usecs / 1000) + (secs * 1000);

	return usecs;
}

static int timer_add_to_list(egg_timer_t* &timer_list, egg_timer_t *timer)
{
	egg_timer_t *prev = NULL, *ptr = NULL;

	/* Find out where this should go in the list. */
	for (ptr = timer_list; ptr; ptr = ptr->next) {
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
		timer->next = timer_list;
		timer_list = timer;
	}
	return(0);
}

int timer_create_secs(int secs, const char *name, Function callback)
{
	egg_timeval_t howlong;

	howlong.sec = secs;
	howlong.usec = 0;

	return timer_create_repeater(&howlong, name, callback);
}

int timer_create_complex(egg_timeval_t *howlong, const char *name, Function callback, void *client_data, int flags)
{
	egg_timer_t *timer = NULL;

	/* Fill out a new timer. */
	timer = (egg_timer_t *) my_calloc(1, sizeof(*timer));
	timer->id = timer_next_id++;
	if (name) timer->name = strdup(name);
	else timer->name = NULL;
	timer->callback = (TimerFunc) callback;
	timer->client_data = client_data;
	timer->flags = flags;
	timer->howlong.sec = howlong->sec;
	timer->howlong.usec = howlong->usec;
	timer->trigger_time.sec = now.sec + howlong->sec;
	timer->trigger_time.usec = now.usec + howlong->usec;
	timer->called = 0;

	if (timer->flags & TIMER_REPEAT)
		timer_add_to_list(timer_repeat_head, timer);
	else
		timer_add_to_list(timer_once_head, timer);

	return(timer->id);
}

static int timer_destroy_list(egg_timer_t* &timer_list, int timer_id)
{
	egg_timer_t *prev = NULL, *timer = NULL;

	prev = NULL;
	for (timer = timer_list; timer; timer = timer->next) {
		if (timer->id == timer_id) break;
		prev = timer;
	}

	if (!timer) return(1); /* Not found! */

	/* Unlink it. */
	if (prev) prev->next = timer->next;
	else timer_list = timer->next;

	if (timer->name)
		free(timer->name);
	free(timer);
	return(0);
}

/* Destroy a timer, given an id. */
int timer_destroy(int timer_id)
{
	if (timer_destroy_list(timer_repeat_head, timer_id))
		if (timer_destroy_list(timer_once_head, timer_id))
			return 1;
	return 0;
}

#ifdef not_used
int timer_destroy_all()
{
	egg_timer_t *timer = NULL, *next = NULL;

	for (timer = timer_list_head; timer; timer = next) {
		next = timer->next;
	}
	timer_list_head = NULL;
	return(0);
}
#endif

int timer_get_shortest(egg_timeval_t *howlong)
{
	egg_timer_t *timer = timer_repeat_head;

	/* No timers? Boo. */
	if (!timer) return(1);

	timer_diff(&now, &timer->trigger_time, howlong);
	return(0);
}

static bool process_timer(egg_timer_t* timer) {
	TimerFunc callback = timer->callback;
	void *client_data = timer->client_data;
	bool deleted = 0;

	if (timer->flags & TIMER_REPEAT) {
		/* Update timer. */
		/* This used to be '+= howlong.sec' but, if the time changed say 3 years (happened), this function
		 * would end up executing all timers for 3 years until it is caught up.
		 */
		timer->trigger_time.sec = now.sec + timer->howlong.sec;
		timer->trigger_time.usec = now.usec + timer->howlong.usec;

		if (timer->trigger_time.usec >= 1000000) {
			timer->trigger_time.usec -= 1000000;
			++(timer->trigger_time.sec);
		}

		++(timer->called);
	} else {
		deleted = 1;
	}

	if (timer->name) {
		simple_snprintf(get_buf[current_get_buf], sizeof(get_buf[current_get_buf]), "Execing timer: %s", timer->name);
		get_buf_inc();
	}

	callback(client_data);
	return deleted;
}

static void process_timer_list(egg_timer_t* &timer_list) {
	egg_timer_t *timer = NULL, *prev = NULL, *next = timer_list;
	while (next) {
		timer = next;
		// Timers are sorted by lowest->highest, so if the current one isn't ready to trigger, the rest are not either
		if (timer->trigger_time.sec > now.sec || (timer->trigger_time.sec == now.sec && timer->trigger_time.usec > now.usec))
			break;
		next = timer->next;
		if (process_timer(timer)) {
			// Deleted, need to shift the queue
			if (prev) prev->next = timer->next;
			else timer_list = timer->next;

			if (timer->name)
				free(timer->name);
			free(timer);
		} else
			prev = timer;
	}
}

void timer_run()
{
	process_timer_list(timer_once_head);
	process_timer_list(timer_repeat_head);
}

int timer_list(int **ids)
{
	egg_timer_t *timer = NULL;
	int ntimers = 0;

	/* Count timers. */
	for (timer = timer_repeat_head; timer; timer = timer->next) ntimers++;

	/* Fill in array. */
	*ids = (int *) my_calloc(1, sizeof(int) * (ntimers+1));
	ntimers = 0;
	for (timer = timer_repeat_head; timer; timer = timer->next) {
		(*ids)[ntimers++] = timer->id;
	}
	return(ntimers);
}

int timer_info(int id, char **name, egg_timeval_t *initial_len, egg_timeval_t *trigger_time, int *called)
{
        egg_timer_t *timer = NULL;

        for (timer = timer_repeat_head; timer; timer = timer->next) {
                if (timer->id == id) break;
        }
        if (!timer) return(-1);
	if (name) *name = timer->name;
        if (initial_len) memcpy(initial_len, &timer->howlong, sizeof(*initial_len));
        if (trigger_time) memcpy(trigger_time, &timer->trigger_time, sizeof(*trigger_time));
	if (called) *called = timer->called;
        return(0);
}

/* vim: set sts=4 sw=4 ts=4 noet: */
