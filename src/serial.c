#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "log.h"
#include "callisto.h"

int serial_debug = 0;
static int ignore_errors = 0;
static int serial_fd = -1;

int init_serial(const char *devname) {
    struct termios tc;

    if ((serial_fd = open(devname, O_RDWR|O_NOCTTY)) < 0) {
        fprintf(stderr, "ERROR: cannot open serial device %s: %s\n",
                devname, strerror(errno));
        return 0;
    }

    /* This prevents *future* open() by *non-root* users. Won't help
       with other callisto instances if they are started as root, for
       that use flock() below. */
#ifdef TIOCEXCL
    if (ioctl(serial_fd, TIOCEXCL)) {
        fprintf(stderr,
		"ERROR: cannot exclusively claim serial device %s: %s\n",
                devname, strerror(errno));
        return 0;
    }
#endif
    
    /* This works for other callisto instances and other programs that
       use flock(), but not for those that (still) use lock files,
       e.g. picocom in Debian Jessie. And then there's the programs
       that don't use locking at all... I won't bother implementing
       other locking mechanisms because there is no guarantee that
       they will work. */
    if (flock(serial_fd, LOCK_EX|LOCK_NB)) {
	fprintf(stderr, "ERROR: cannot lock serial device %s: %s\n",
		devname, strerror(errno));
	return 0;
    }

    if (tcgetattr(serial_fd, &tc)) {
        fprintf(stderr, "ERROR: getting serial settings for %s failed: %s\n",
                devname, strerror(errno));
        return 0;
    }

    /* configure as 115200 8N1 no flow control */
    cfsetspeed(&tc, B115200);
    cfmakeraw(&tc);
    tc.c_iflag &= ~(INPCK | IGNPAR | IXOFF | IMAXBEL | IUCLC);
    tc.c_iflag |= (IGNBRK | IXANY);
    tc.c_oflag &= ~(ONLCR | XTABS | OLCUC);
    tc.c_cflag &= ~(HUPCL | CRTSCTS | CSTOPB);
    tc.c_cflag |= (CLOCAL | CREAD);
    tc.c_lflag &= ~(TOSTOP);
    tc.c_cc[VMIN] = 0;  /* non-blocking reads */
    tc.c_cc[VTIME] = 10; /* wait at most 1s for data to read */

    if (tcsetattr(serial_fd, TCSAFLUSH, &tc)) {
        fprintf(stderr, "ERROR: changing serial settings for %s failed: %s\n",
                devname, strerror(errno));
        return 0;
    }

    return 1;
}

int read_serial(char *c) {
    ssize_t r;
    while ((r = read(serial_fd, c, 1)) != 1) {
	if (r == 0)
	    return 0;
	if (errno != EINTR) {
	    if (ignore_errors)
		return 0;
	    logprintf(LOG_EMERG, "Error reading serial port: %s",
		      strerror(errno));
	    logprintf(LOG_EMERG, "Terminating due to I/O error");
	    ignore_errors = 1;
	    terminate(0);
	}
    }
    if (serial_debug) {
	if (serial_debug > 1)
	    fputs("\033[01;34m", stderr);
	fputc(*c == '\r' ? '\n' : *c, stderr);
	if (serial_debug > 1)
	    fputs("\033[0m", stderr);
    }

    return 1;
}

int write_serial(const char *s) {
    ssize_t w, left = strlen(s);
    const char *ss = s;
    while (left > 0) {
	w = write(serial_fd, s, left);
	if (w < 0) {
	    if (errno != EINTR) {
		if (ignore_errors)
		    return 0;
		logprintf(LOG_EMERG, "Error writing serial port: %s",
			  strerror(errno));
		logprintf(LOG_EMERG, "Terminating due to I/O error");
		ignore_errors = 1;
		terminate(0);
	    }
	    w = 0;
	}
	left -= w;
	s += w;
    }
    tcdrain(serial_fd);

    if (serial_debug) {
	if (serial_debug > 1)
	    fputs("\033[01;31m", stderr);
	while (*ss) {
	    fputc(*ss == '\r' ? '\n' : *ss, stderr);
	    ss++;
	}
	if (serial_debug > 1)
	    fputs("\033[0m", stderr);
    }

    return 1;
}
