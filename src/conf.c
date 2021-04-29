#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "conf.h"
#include "log.h"
#include "util.h"
#include "callisto.h"

config_t config;
channel_t channels[MAX_CHANNELS];

#define MAXLINE 1024

/* if key==NULL return "raw" values */
static int getconf(FILE *f, char **key, char **value) {
    static char buffer[MAXLINE];
    char *k, *v;
    int i, l;

    while (fgets(buffer, MAXLINE, f)) {
	l = strlen(buffer);
	if (buffer[l-1] != '\n') {
	    fprintf(stderr, "ERROR: Line too long\n");
	    return 0;
	}
	
	/* trim leading whitespace: */
	k = &buffer[0];
	k += strspn(k, " \t\r\n");
	
	/* remove comments: */
	v = strstr(k, "//");
	if (v != NULL) *v = 0;
	v = strstr(k, "/*");
	if (v != NULL) *v = 0;
	
	/* trim trailing whitespace: */
	l = strlen(k);
	while (l && strchr(" \t\r\n", k[l-1]) != NULL) {
	    l--;
	    k[l] = 0;
	}

	if (!*k) /* empty line */
	    continue;

	/* return line relevant content without key/value parse or
	   lowercase conversion: */
	if (key == NULL) {
	    *value = k;
	    return 1;
	}

	if (*k != '[') {
	    fprintf(stderr, "WARNING: malformed configuration file line\n");
	    continue;
	}
	k++;


	v = k;
	while (*v && *v != ']')
	    v++;

	if (!*v) {
	    fprintf(stderr, "WARNING: malformed configuration file line\n");
	    continue;
	}
	*v = 0;
	v++;

	v += strspn(v, " \t\r\n");
	if (*v != '=') {
	    fprintf(stderr, "WARNING: malformed configuration file line\n");
	    continue;
	}
	v++;
	v += strspn(v, " \t\r\n");

	for (i = 0; k[i]; i++)
	    if (k[i] >= 'A' && k[i] <= 'Z')
		k[i] += ('a' - 'A');

	*key = k;
	*value = v;
	return 1;
    }
    return 0;
}

int read_config(const char *fname) {
    FILE *f;
    char *key, *value;
    int nconf = 0;
    int mmode = 3;

    if ((f = fopen(fname, "r")) == NULL) {
	fprintf(stderr, "ERROR: Cannot open configuration file %s: %s\n",
		fname, strerror(errno));
	return 0;
    }

    memset(&config, 0, sizeof(config));
    /* defaults: */
    config.chargepump = 1;
    config.agclevel = 120;
    config.local_oscillator = 0.0;
    config.clocksource = 1;
    config.ovsdir = NULL;
    config.schedulefile = NULL;
    config.autostart = -1; /* 0=no, 1=yes, -1=by schedule */

    config.net_port = 0;

    while (getconf(f, &key, &value)) {
	
	if (!strcmp(key, "rxcomport")) {
	    config.serialport = strdup(value);
	    if (config.serialport) nconf++;
	} else if (!strcmp(key, "instrument")) {
	    config.instrument = strdup(value);
	    if (config.instrument) nconf++;
	} else if (!strcmp(key, "origin")) {
	    config.origin = strdup(value);
	    if (config.origin) nconf++;
	} else if (!strcmp(key, "frqfile")) {
	    config.channelfile = strdup(value);
	    if (config.channelfile) nconf++;
	} else if (!strcmp(key, "datapath")) {
	    config.datadir = strdup(value);
	    if (config.datadir) nconf++;
	} else if (!strcmp(key, "ovspath")) {
	    config.ovsdir = strdup(value);
	} else if (!strcmp(key, "longitude")) {
	    char c;
	    sscanf(value, "%c , %lf", &c, &config.obs_long);
	    if (c == 'W' || c == 'w')
		config.obs_long = -config.obs_long;
	    if (strchr("EeWw", c)) nconf++;
	} else if (!strcmp(key, "latitude")) {
	    char c;
	    sscanf(value, "%c , %lf", &c, &config.obs_lat);
	    if (c == 'S' || c == 's')
		config.obs_lat = -config.obs_lat;
	    if (strchr("NnSs", c)) nconf++;
	} else if (!strcmp(key, "height")) {
	    config.obs_height = atof(value);
	    nconf++;
	} else if (!strcmp(key, "chargepump")) {
	    config.chargepump = !!atoi(value);
	} else if (!strcmp(key, "agclevel")) {
	    config.agclevel = atoi(value);
	} else if (!strcmp(key, "clocksource")) {
	    config.clocksource = atoi(value);
	} else if (!strcmp(key, "filetime")) {
	    config.filetime = atoi(value);
	    nconf++;
	} else if (!strcmp(key, "focuscode")) {
	    config.focuscode = atoi(value);
	    nconf++;
	} else if (!strcmp(key, "autostart")) {
	    config.autostart = atoi(value);
	} else if (!strcmp(key, "net_port")) {
	    config.net_port = atoi(value);
	} else if (!strcmp(key, "mmode")) {
	    mmode = atoi(value);
	}
    }
    fclose(f);
    
    if (nconf != 10) {
	fprintf(stderr,
		"ERROR: Some configuration variables are missing from %s\n",
		fname);
	return 0;
    }

    if (mmode != 3) {
	fprintf(stderr,
		"ERROR: Measuring mode %i not supported, "
		"set in configuration file %s\n",
		mmode, fname);
	return 0;
    }

    if (config.ovsdir == NULL)
	config.ovsdir = config.datadir;
    if (config.schedulefile == NULL)
	config.schedulefile = "scheduler.cfg";

    return 1;
}


int read_channels(const char *fname) {
    FILE *f;
    char *key, *value;
    int targetok = 0;
    int nsweeps = 0;
    int i;

    if ((f = fopen(fname, "r")) == NULL) {
	fprintf(stderr, "ERROR: Cannot open channel file %s: %s\n",
		fname, strerror(errno));
	return 0;
    }

    memset(&channels, 0, MAX_CHANNELS * sizeof(channel_t));

    while (getconf(f, &key, &value)) {

	if (!strcmp(key, "target")) {
	    if (!strcasecmp(value, "CALLISTO"))
		targetok = 1;
	} else if (!strcmp(key, "number_of_measurements_per_sweep")) {
	    config.nchannels = atoi(value);
	    if (config.nchannels < 1 || config.nchannels > 512) {
		fprintf(stderr, "ERROR: Invalid number of channels: %i\n",
			config.nchannels);
		return 0;
	    }
	} else if (!strcmp(key, "number_of_sweeps_per_second")) {
	    nsweeps = atoi(value);
	} else if (!strcmp(key, "external_lo")) {
	    config.local_oscillator = atof(value);
	} else {
	    int ch;
	    char *e;
	    ch = strtol(key, &e, 10);
	    if (!*e) {
		if (ch < 1 || ch > 512) {
		    fprintf(stderr, "ERROR: Channel out of range in %s (%i)\n",
			    fname, ch);
		} else {
		    ch--;
		    if (sscanf(value, "%lf , %d",
			       &channels[ch].f, &channels[ch].lc) == 2)
			channels[ch].valid = 1;
		    else
			fprintf(stderr,
				"WARNING: Bad channel definition: %s\n", value);
		}
	    }
	}
    }
    fclose(f);

    if (!targetok) {
	fprintf(stderr, "ERROR: Channel file %s is not for Callisto\n", fname);
	return 0;
    }


    config.samplerate = nsweeps * config.nchannels;
    if (config.samplerate < 1 || config.samplerate > 1000) {
	fprintf(stderr, "ERROR: Sample rate out of range in %s (%i)\n",
		fname, config.samplerate);
	return 0;
    }

    for (i = 0; i < config.nchannels; i++)
	if (!channels[i].valid) {
	    fprintf(stderr, "ERROR: Channel definitions missing from %s\n",
		    fname);
	    return 0;
	}

    return 1;
}




int numschedule = 0;
schedule_t schedule[MAX_SCHEDULE];

int read_schedule(const char *fname) {
    FILE *f;
    char *s;
    time_t now = time(NULL);

    if ((f = fopen(fname, "r")) == NULL) {
	if (errno == ENOENT)
	    return 2;
	logprintf(LOG_ERR, "Cannot open schedule file %s: %s",
		   fname, strerror(errno));
	return 0;
    }

    if (debug)
	logprintf(LOG_DEBUG, "Reading schedule file %s", fname);

    numschedule = 0;
    while (getconf(f, NULL, &s)) {
	int hour, min, sec, fc, mode;
	time_t t;
	struct tm tm;

	if (sscanf(s, "%d:%d:%d,%d,%d", &hour, &min, &sec, &fc, &mode) != 5) {
	    logprintf(LOG_WARNING,
		      "Malformed schedule file entry in file %s: %s",
		      fname, s);
	    continue;
	}
	
	if (hour < 0 || hour > 23
	    || min < 0 || min > 59
	    || sec < 0 || sec > 59) {
	    logprintf(LOG_WARNING,
		      "Invalid schedule timestamp in file %s: %s",
		      fname, s);
	    continue;
	}

	/* This program does not support switching focuscodes: */
	if (fc != config.focuscode) {
	    if (debug)
		logprintf(LOG_DEBUG,
			  "Skipping schedule entry of not our focus code: %s",
			  s);
	    continue;
	}

	if (mode != SCHEDULE_STOP
	    && mode != SCHEDULE_START
	    && mode != SCHEDULE_OVERVIEW) {
	    logprintf(LOG_WARNING,
		      "Unsupported measuring mode %i in schedule file %s: %s",
		      mode, fname, s);
	    continue;
	}

	if (numschedule >= MAX_SCHEDULE) {
	    logprintf(LOG_WARNING, "Too many entries in schedule file %s",
		      fname);
	    break;
	}

	gmtime_r(&now, &tm);
	tm.tm_sec = sec;
	tm.tm_min = min;
	tm.tm_hour = hour;

	t = timegm(&tm);

	if (t < now)
	    t += 86400;

	schedule[numschedule].t = t;
	schedule[numschedule].action = mode;
	numschedule++;

	if (debug)
	    logprintf(LOG_DEBUG, "Added schedule entry: %s", s);
    }
    fclose(f);

    if (!numschedule)
	logprintf(LOG_WARNING, "Loaded schedule is empty");

    if (config.autostart < 0) {
	/* determine what our starting mode (on/off) should be */
	int i;
	time_t max = 0;
	int n = -1;

	if (debug)
	    logprintf(LOG_DEBUG, "Configuring autostart from schedule");

	for (i = 0; i < numschedule; i++)
	    if (schedule[i].t > max
		&& (schedule[n].action == SCHEDULE_START
		    || schedule[n].action == SCHEDULE_STOP))
		{
		    max = schedule[i].t;
		    n = i;
		}

	if (n >= 0) {
	    if (schedule[n].action == SCHEDULE_START)
		config.autostart = 1;
	    else
		config.autostart = 0;
	}
    }

    return 1;
}
