#ifndef CALLISTO_LOG_H
#define CALLISTO_LOG_H

#include <syslog.h>

void log_init(int isdaemon);
void logprintf(int priority, const char *format, ...);

#endif
