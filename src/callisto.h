#ifndef CALLISTO_CALLISTO_H
#define CALLISTO_CALLISTO_H

#include <inttypes.h>

#include "util.h"

#define MAX_MESSAGE 128

/* firmware special characters */
#define MESSAGE_START '$'
#define MESSAGE_END '\r'
#define DATA_START '2'
#define DATA_END '&'
#define EEPROM_READY ']'

typedef struct {
    double if_init;
    double if_init_correction;
    int data10bit;
    int eeprom_info;
    char *versionstr;
} firmware_t;
extern firmware_t firmware;

extern int debug;

typedef struct {
    uint8_t *data;
    volatile int size;
    usec_t timestamp;  /* timestamp of the first sample */
} buffer_t;

extern int buffer_size;
extern buffer_t buffer[2];
extern volatile int save_buffer;
extern volatile int active_buffer;

extern volatile int start_command, overview_command, stop_command;

void terminate(int signum);

#endif
