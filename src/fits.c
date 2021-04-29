#include <config.h>

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <fitsio.h>
#include <inttypes.h>

#include "conf.h"
#include "util.h"
#include "callisto.h"
#include "log.h"

static int image_w = 0, image_h = 0;
static uint8_t *image_buffer = NULL;
static double *image_time = NULL, *image_freq = NULL;

static int write_fits(int buf)	{
    char s[PATH_MAX];
    long naxes[2] = { image_w, image_h };
    long minvalue = 255, maxvalue = 0, l;
    double d;
    int status = 0;
    fitsfile *fptr;
    struct tm t, et;
    time_t ut;
    usec_t ets;
    int x, y;
    char* tType[2] = { "TIME", "FREQUENCY" };
    char tForm_0[32], tForm_1[32];
    char* tForm[2] = { tForm_0, tForm_1 };
    double dt = 1.0 / ((double)config.samplerate / (double)config.nchannels);
    char errstr[FLEN_STATUS];
    
    ut = buffer[buf].timestamp / 1000000;
    gmtime_r(&ut, &t);
    ets = buffer[buf].timestamp
	  + 1000000 * (usec_t)(image_w*image_h) / (usec_t)config.samplerate;
    ut = ets / 1000000;
    gmtime_r(&ut, &et);

    snprintf(s, PATH_MAX, "!%s/%s_%04u%02u%02u_%02u%02u%02u_%02u.fit",
	     config.datadir, config.instrument,
	     t.tm_year+1900, t.tm_mon+1, t.tm_mday,
	     t.tm_hour, t.tm_min, t.tm_sec,
	     config.focuscode);

    if (debug)
	logprintf(LOG_DEBUG, "Writing FITS file %s", s+1);
    
    fits_create_file(&fptr, s, &status);
    
    fits_create_img(fptr, BYTE_IMG, 2, naxes, &status);

    fits_write_comment(fptr, " File created by e-Callisto for Unix version "
		       PACKAGE_VERSION, &status);

    sprintf(s, "%04u-%02u-%02u", t.tm_year+1900, t.tm_mon+1, t.tm_mday);
    fits_update_key(fptr, TSTRING, "DATE", s, "Time of observation",
		    &status);
    sprintf(s, "%04u/%02u/%02u  Radio flux density, e-CALLISTO (%s)",
	    t.tm_year+1900, t.tm_mon+1, t.tm_mday, config.instrument);
    fits_update_key(fptr, TSTRING, "CONTENT", s, "Title of image",
		    &status);

    fits_update_key(fptr, TSTRING, "ORIGIN", (char*)config.origin,
		    "Organization name", &status);
    fits_update_key(fptr, TSTRING, "TELESCOP", "Radio Spectrometer",
		    "Type of instrument", &status);
    fits_update_key(fptr, TSTRING, "INSTRUME", (char*)config.instrument,
		    "Name of the spectrometer", &status);
    fits_update_key(fptr, TSTRING, "OBJECT", "Sun",
		    "object description", &status);

    sprintf(s, "%04u/%02u/%02u",
	    t.tm_year+1900, t.tm_mon+1, t.tm_mday);
    fits_update_key(fptr, TSTRING, "DATE-OBS", s,
		    "Date observation starts", &status);
    sprintf(s, "%02u:%02u:%02u.%03u", t.tm_hour, t.tm_min, t.tm_sec,
	    (unsigned)((buffer[buf].timestamp / 1000) % 1000));
    fits_update_key(fptr, TSTRING, "TIME-OBS", s,
		    "Time observation starts", &status);


    sprintf(s, "%04u/%02u/%02u", et.tm_year+1900, et.tm_mon+1, et.tm_mday);
    fits_update_key(fptr, TSTRING, "DATE-END", s,
		    "date observation ends", &status);
    sprintf(s, "%02u:%02u:%02u", et.tm_hour, et.tm_min, et.tm_sec);
    fits_update_key(fptr, TSTRING, "TIME-END", s,
		    "time observation ends", &status);

    /* FITS image is transposed and vertically mirrored: */
    for (x = 0; x < image_w; x++)
	for (y = 0; y < image_h; y++)
	    image_buffer[y*image_w + x]
		= buffer[buf].data[x*image_h + image_h-1-y];

    /* find min and max pixels: */
    for (x = 0; x < image_w*image_h; x++) {
	if (image_buffer[x] > maxvalue)
	    maxvalue = image_buffer[x];
	if (image_buffer[x] < minvalue)
	    minvalue = image_buffer[x];
    }

    d = 0.0;
    fits_update_key(fptr, TDOUBLE, "BZERO", &d, "scaling offset", &status);
    d = 1.0;
    fits_update_key(fptr, TDOUBLE, "BSCALE", &d, "scaling factor", &status);

    /* uncalibrated data, units are "ADU": */
    fits_update_key(fptr, TSTRING, "BUNIT", "digits", "z-axis title", &status);

    fits_update_key(fptr, TLONG, "DATAMIN", &minvalue,
		    "minimum element in image", &status);
    fits_update_key(fptr, TLONG, "DATAMAX", &maxvalue,
		    "maximum element in image", &status);

    d = 3600.0*t.tm_hour + 60.0*t.tm_min + 1.0*t.tm_sec;
    fits_update_key(fptr, TDOUBLE, "CRVAL1", &d,
		    "value on axis 1 at reference pixel [sec of day]", &status);
    l = 0;
    fits_update_key(fptr, TLONG, "CRPIX1", &l,
		    "reference pixel of axis 1", &status);
    fits_update_key(fptr, TSTRING, "CTYPE1", "Time [UT]",
		    "title of axis 1", &status);
    fits_update_key(fptr, TDOUBLE, "CDELT1", &dt,
		    "step between first and second element in x-axis [sec]",
		    &status);

    d = (double)config.nchannels;
    fits_update_key(fptr, TDOUBLE, "CRVAL2", &d,
		    "value on axis 2 at the reference pixel", &status);
    l = 0;
    fits_update_key(fptr, TLONG, "CRPIX2", &l,
		    "reference pixel of axis 2", &status);
    fits_update_key(fptr, TSTRING, "CTYPE2", "Frequency [MHz]",
		    "title of axis 2", &status);
    d = -1.0;
    fits_update_key(fptr, TDOUBLE, "CDELT2", &d,
		    "step between first and second element in y-axis", &status);

    fits_write_comment(fptr, " Warning: the value of CDELT1 may be rounded!",
		       &status);
    fits_write_comment(fptr, " Warning: the frequency axis may not be regular!",
		       &status);
    fits_write_comment(fptr, " Warning: the value of CDELT2 may be rounded!",
		       &status);


    d = fabs(config.obs_lat);
    fits_update_key(fptr, TDOUBLE, "OBS_LAT", &d,
		    "observatory latitude in degree", &status);
    sprintf(s,"%c", config.obs_lat < 0.0 ? 'S' : 'N');
    fits_update_key(fptr, TSTRING, "OBS_LAC", s,
		    "observatory latitude code {N,S}", &status);
    d = fabs(config.obs_long);
    fits_update_key(fptr, TDOUBLE, "OBS_LON", &d,
		    "observatory longitude in degree", &status);
    sprintf(s,"%c", config.obs_long < 0.0 ? 'W' : 'E');
    fits_update_key(fptr, TSTRING, "OBS_LOC", s,
		    "observatory longitude code {E,W}", &status);
    fits_update_key(fptr, TDOUBLE, "OBS_ALT", &config.obs_height,
		    "observatory altitude in meter asl", &status);

    fits_update_key(fptr, TSTRING, "FRQFILE", (char*)config.channelfile,
		    "name of frequency file" , &status);
    l = config.agclevel;
    fits_update_key(fptr, TLONG, "PWM_VAL", &l,
		    "PWM value to control tuner gain", &status);

    /* fits_update_key(fptr, TSTRING, "HISTORY", "", "", &status); */

    fits_write_img(fptr, TBYTE, 1, image_w*image_h, image_buffer, &status);


    sprintf(tForm[0],"%dD8.3", image_w);
    sprintf(tForm[1],"%dD8.3", image_h);

    fits_create_tbl(fptr, BINARY_TBL, 0, 2, tType, tForm, NULL, NULL, &status);

    d = 1.0;
    fits_write_key(fptr, TDOUBLE, "TSCAL1", &d, NULL, &status);
    fits_write_key(fptr, TDOUBLE, "TSCAL2", &d, NULL, &status);
    d = 0.0;
    fits_write_key(fptr, TDOUBLE, "TZERO1", &d, NULL, &status);
    fits_write_key(fptr, TDOUBLE, "TZERO2", &d, NULL, &status);


    for (x = 0; x < image_w; x++)
	image_time[x] = x * dt;
    fits_write_col(fptr, TDOUBLE, 1, 1, 1, image_w, image_time, &status);

    for (x = 0; x < image_h; x++)
	image_freq[image_h-1-x] = channels[x].f;
    fits_write_col(fptr, TDOUBLE, 2, 1, 1, image_h, image_freq, &status);

    fits_close_file(fptr, &status);

    if (status != 0) {
	fits_get_errstatus(status, errstr);
	logprintf(LOG_ERR, "FITS write failed: %s", errstr);
    }

    return (status == 0);
}



static pthread_t thread_id;
static void *fitswriter(void *dummy) {
    (void)dummy;

    while (1) {
	while (save_buffer == -1) /* wait for buffer fill */
	    msleep(100);

	/* update w to support incomplete images: */
	image_w = buffer[save_buffer].size / config.nchannels;

	if (image_w)
	    write_fits(save_buffer);

	save_buffer = -1; /* done */
    }

    return NULL;
}

int fits_init() {
    /* h and initial w, for memory allocation: */
    image_h = config.nchannels;
    image_w = buffer_size / config.nchannels;

    image_buffer =
	(uint8_t*)malloc(image_w * image_h);
    image_time = (double*)malloc(image_w * sizeof(double));
    image_freq = (double*)malloc(image_h * sizeof(double));

    if (!image_buffer || !image_time || !image_freq) {
	fprintf(stderr, "ERROR: Cannot allocate image buffers\n");
	return 0;
    }

    return 1;
}

void fits_start() {
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0
        || pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0
        || pthread_create(&thread_id, &attr, fitswriter, NULL) != 0
        || pthread_attr_destroy(&attr) != 0) {
	logprintf(LOG_CRIT,
		  "Cannot create FITS writer thread, terminating: %s",
		  strerror(errno));
        terminate(-1);
    }

}
