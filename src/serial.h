#ifndef CALLISTO_SERIAL_H
#define CALLISTO_SERIAL_H

int init_serial(const char *devname);
int read_serial(char *c);
int write_serial(const char *s);

extern int serial_debug;

#endif
