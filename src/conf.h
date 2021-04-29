#ifndef CALLISTO_CONF_H
#define CALLISTO_CONF_H

#include <time.h>

/* Channels are stored in 4096-byte EEPROM, 8 bytes per channel: */
#define MAX_CHANNELS 512

/* This is the same as in the Windows host software: */
#define MAX_SCHEDULE 150

typedef struct {
    const char *serialport;
    const char *instrument;
    const char *origin;
    const char *channelfile;
    const char *datadir;
    const char *ovsdir;
    const char *schedulefile;
    double obs_long, obs_lat, obs_height;
    double local_oscillator; /* MHz */
    int chargepump;
    int agclevel;
    int clocksource;
    int filetime;
    int focuscode;
    int nchannels;       /* = sweep length */
    int samplerate;      /* samples / sec */
    int autostart;

    int net_port;
} config_t;

extern config_t config;

typedef struct {
    int valid;
    double f; /* frequency, in MHz */
    int lc;   /* number of integrations for lightcurves */
} channel_t;

extern channel_t channels[MAX_CHANNELS];

/* Supported callisto operating modes: */
#define SCHEDULE_STOP 0
#define SCHEDULE_START 3
#define SCHEDULE_OVERVIEW 8

typedef struct {
    time_t t;
    int action;
} schedule_t;

extern schedule_t schedule[MAX_SCHEDULE];
extern int numschedule;

int read_config(const char *fname);
int read_channels(const char *fname);
int read_schedule(const char *fname);

#endif
