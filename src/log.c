#include <config.h>

#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static int use_stderr = 1;

void log_init(int isdaemon) {
    use_stderr = !isdaemon;
    if (!use_stderr)
	openlog(PACKAGE_NAME, LOG_NDELAY | LOG_PID, LOG_DAEMON);
}

void logprintf(int priority, const char *format, ...) {
    va_list args;
    time_t now;
    struct tm nowtm;

    va_start(args, format);

    if (use_stderr) {
	now = time(NULL);
	localtime_r(&now, &nowtm);
	fprintf(stderr, "[%.4i-%.2i-%.2i %.2i:%.2i:%.2i] ",
		nowtm.tm_year + 1900, nowtm.tm_mon + 1, nowtm.tm_mday,
		nowtm.tm_hour, nowtm.tm_min, nowtm.tm_sec);
	vfprintf(stderr, format, args);
	fputc('\n', stderr);
    } else
	vsyslog(priority, format, args);

    va_end(args);
}
