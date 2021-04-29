#ifndef CALLISTO_UTIL_H
#define CALLISTO_UTIL_H

#include <config.h>

#include <inttypes.h>
#include <time.h>

typedef int64_t usec_t;
#define USEC_MAX INT64_MAX
#define USEC_MIN INT64_MIN

/* Get current unix time in microseconds */
usec_t get_usecs();

/* Wrapper to nanosleep(2) that handles EINTR */
void nsleep(uint64_t nsecs);
#define msleep(x) nsleep((uint64_t)(x)*1000000LL)
#define microsleep(x) nsleep((uint64_t)(x)*1000LL)

#if !HAVE_TIMEGM
time_t timegm(struct tm *tm);
#endif

int daemonize();

#endif
