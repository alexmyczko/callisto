#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "callisto.h"
#include "conf.h"
#include "serial.h"
#include "log.h"

/* channel step, MHz: */
#define SYNTHESIZER_RESOLUTION 0.0625

/* Default to firmware version 1.5 (of 1.5/1.7/1.8): */
firmware_t firmware = {
    /* IF-frequency #1 = IF-frequency #2 + Quarz-frequency [MHz]
       (10.70+27.0 for firmware 1.5/1.7). */
    .if_init = 37.7,
    .if_init_correction = 0.05,
    .data10bit = 0,
    .eeprom_info = 0,
    .versionstr = "1.5"
};

/* Frequency band limits: */
#define LOW_BAND 171.0
#define MID_BAND 450.0
#define HIGH_BAND 870.0
int upload_channels() {
    int i;

    if (debug)
	logprintf(LOG_DEBUG, "Uploading %i channels", config.nchannels);

    for (i = 0; i < config.nchannels; i++) {
	unsigned divider, div_hi, div_lo, control = 0x86, band = 1;
	double f = fabs(channels[i].f - config.local_oscillator);
	char cmd[32];
	char c = 0;

	divider = (unsigned)((f + firmware.if_init) / SYNTHESIZER_RESOLUTION);
	div_hi = (divider >> 8) & 0xff;
	div_lo = divider & 0xff;

	if (config.chargepump)
	    control |= 0x40;

	if (f < LOW_BAND)
	    band = 1;
	else if (f < MID_BAND)
	    band = 2;
	else /* f < HIGH_BAND */
	    band = 4;

	sprintf(cmd, "FE%u,%03u,%03u,%03u,%03u\r",
		i+1, div_hi, div_lo, control, band);
	write_serial(cmd);
	while (c != EEPROM_READY) {
	    if (!read_serial(&c)) {
		fprintf(stderr, "ERROR: Timeout while uploading channels\n");
		return 0;
	    }
	}
    }

    return 1;
}

int download_channels() {
    char msg[MAX_MESSAGE];
    char c = 0;
    int i, ch, d1, d2;
    double d;

    if (debug)
	logprintf(LOG_DEBUG, "Downloading %i channels", config.nchannels);

    /* Turn on debugging: */
    write_serial("D1\r");
    /* eat the response line: */
    while (c != '\r')
	if (!read_serial(&c)) {
	    fprintf(stderr,
		    "ERROR: Timeout reading expected message\n");
	    return 0;
	}
    /* and the command echo: */
    c = 0;
    while (c != '\r')
	if (!read_serial(&c)) {
	    fprintf(stderr,
		    "ERROR: Timeout reading expected message\n");
	    return 0;
	}

    for (ch = 0; ch < config.nchannels; ch++) {
	/* Query the channel: */
	sprintf(msg, "FR%i\r", ch+1);
	write_serial(msg);

	/* read the answer line: */
	i = 0;
	c = 0;
	while (c != '\r' && i < MAX_MESSAGE && read_serial(&c)) {
	    msg[i] = c;
	    i++;
	}
	if (i == MAX_MESSAGE) i--;
	msg[i] = 0;
	
	if (msg[i-1] != '\r') {
	    fprintf(stderr,
		    "ERROR: Incomplete line while downloading channels: %s\n",
		    msg);
	    return 0;
	}

	/* The firmware prints the decimal part without leading
	   zeroes. Also, firmware version 1.5 uses 37.75 as IF_INIT in
	   the response to the FRxxx command, so it needs to be
	   corrected here by +0.05 */
	if (sscanf(msg, "$CRX:Frequency%c%d.%dMHz", &c, &d1, &d2) != 3) {
	    fprintf(stderr, "ERROR: Invalid response to FR command: %s\n", msg);
	    return 0;
	}
	if (c != '~' && c != '=') {
	    fprintf(stderr, "ERROR: Invalid response to FR command: %s\n", msg);
	    return 0;
	}
	if ((c == '~' && firmware.eeprom_info)
	    ||
	    (c == '=' && !firmware.eeprom_info)) {
	    fprintf(stderr, "ERROR: Firmware mismatch detected\n");
	    return 0;
	}
	if (firmware.eeprom_info) {
	    /* eat the "EEPROM=aaa,bbb,ccc,ddd" info line */
	    c = 0;
	    while (c != '\r')
		if (!read_serial(&c)) {
		    fprintf(stderr,
			    "ERROR: Timeout reading expected message\n");
		    return 0;
		}
	}

	d = (double)(d1*1000+d2)/1000.0 + firmware.if_init_correction;

	/* Compensate for config.local_oscillator: */
	{
	    /* d == fabs(f - config.local_oscillator) */
	    double f1 = d + config.local_oscillator,
		f2 = config.local_oscillator - d;
	    if (f2 < 0.0) f2 = f1;
	    /* select the one closest to the configured frequency: */
	    if (fabs(channels[ch].f - f1) <= fabs(channels[ch].f - f2))
		d = f1;
	    else
		d = f2;
	}

	/* check the downloaded frequency against the
	   configured(/uploaded) one (the one percent fudge factor is
	   there for possible floating point rounding errors): */
	if (fabs(d - channels[ch].f) > 1.01*SYNTHESIZER_RESOLUTION) {
	    fprintf(stderr,
		    "ERROR: Frequency of channel %i differs from its "
		    "configured value. Perhaps channels need to be loaded "
		    "into the EEPROM (e.g. with options '-LC')?\n",
		    ch+1);
	    write_serial("D0\r");
	    return 0;
	}

	/* Store frequency: */
	channels[ch].f = d;

	/* eat the command echo: */
	c = 0;
	while (c != '\r')
	    if (!read_serial(&c)) {
		fprintf(stderr,
			"ERROR: Timeout reading expected message\n");
		return 0;
	    }
    }

    /* Turn off debugging: */
    write_serial("D0\r");

    return 1;
}
