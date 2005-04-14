#ifndef _TIMESPEC_H
#define _TIMESPEC_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef HAVE_TIMESPEC
struct timespec {
    time_t      tv_sec;
    long        tv_nsec;
};
#endif /* !HAVE_TIMESPEC */

#ifndef timespecclear
# define timespecclear(ts)      (ts)->tv_sec = (ts)->tv_nsec = 0
#endif
#ifndef timespecisset
# define timespecisset(ts)      ((ts)->tv_sec || (ts)->tv_nsec)
#endif
#ifndef timespecsub
# define timespecsub(minuend, subrahend, difference)                           \
    do {                                                                       \
            (difference)->tv_sec = (minuend)->tv_sec - (subrahend)->tv_sec;    \
            (difference)->tv_nsec = (minuend)->tv_nsec - (subrahend)->tv_nsec; \
            if ((difference)->tv_nsec < 0) {                                   \
                    (difference)->tv_nsec += 1000000000L;                      \
                    (difference)->tv_sec--;                                    \
            }                                                                  \
    } while (0)
#endif

#endif /* !_TIMESPEC_H */
