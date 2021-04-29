#include <config.h>

#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include "util.h"

usec_t get_usecs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return 1000000LL*(usec_t)tv.tv_sec + (usec_t)tv.tv_usec;
}

void nsleep(uint64_t nsecs) {
    struct timespec ts;
    ts.tv_sec = nsecs / 1000000000LL;
    ts.tv_nsec = nsecs % 1000000000LL;
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) ;
}

/* A portable timegm(2): */
#if !HAVE_TIMEGM
time_t timegm(struct tm *tm) {
    time_t ret;
    char *tz;
    
    tz = getenv("TZ");
    setenv("TZ", "", 1);
    tzset();
    ret = mktime(tm);
    if (tz)
	setenv("TZ", tz, 1);
    else
	unsetenv("TZ");
    tzset();
    return ret;
}
#endif


int daemonize() {
    pid_t pid;
    int fd;

    if ((pid = fork()) == -1) {
        return -1;
    } else if (pid != 0) {
        _exit(EXIT_SUCCESS); /* terminate parent */
    }

    if ((fd = open("/dev/null", O_RDWR)) < 0)
	return -1;

    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    /* setsid() should not fail (and if it does, we don't really care) */
    setsid();

    return 0;
}
