/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Copyright (c) 2017, Thomas Stibor <thomas@stibor.net>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <tiffio.h>
#include <time.h>
#include <errno.h>
#include <fitsio.h>
#include "asi_util.h"
#include "log.h"

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "NA"
#endif

#define MAX_IMG_TYPE_LENGTH	5 /* RAW8, RAW16, RGB24, Y8 */
#define MAX_LEN_ISO8601 32

#define FITS_ERROR(status) 						\
do {		   							\
	if (!status)							\
		fprintf(stderr, "\n");					\
	else {								\
		char status_str[FLEN_STATUS] = {0};			\
		char errmsg[FLEN_ERRMSG] = {0};				\
									\
		/* Get the error description. */			\
		fits_get_errstatus(status, status_str);			\
		fprintf(stderr, RED "[FITS ERROR] " RESET		\
			"%f [%ld] %s:%d %d: ",				\
			c_now(), syscall(SYS_gettid), __FILE__,		\
			__LINE__, status);				\
		/* Get error stack messages. */				\
		while (fits_read_errmsg(errmsg))			\
			fprintf(stderr, "%s", errmsg);			\
		fprintf(stderr, "\n");					\
	}								\
} while (0)

typedef enum {
	TYPE_UNKNOWN = 0,
	TYPE_FIT     = 1,
	TYPE_TIF     = 2
} img_outtype_e;

static img_outtype_e img_outtype = TYPE_UNKNOWN;

const char *BOOL_NO_YES[]   = {"no", "yes"};
const char *BOOL_STR[]	    = {"false", "true"};
const char *BAYER_PATTERN[] = {"RG","BG","GR","GB"};
const char *IMG_TYPE[]	    = {"RAW8", "RGB24", "RAW16", "Y8"};

struct param_val {
	char param[MAX_PV_LENGTH + 1];
	char val[MAX_PV_LENGTH + 1];
};

struct params_vals {
	uint8_t N;
	struct param_val *pv;
};

struct options {
	bool o_list;
	bool o_capa;
	int o_cam_id;
	char o_get[MAX_PV_LENGTH + 1];
	char o_set[MAX_PV_SET_LENGTH + 1];
	bool o_capture;
	int o_width;
	int o_height;
	int o_binning;
	char o_img_type_s[MAX_IMG_TYPE_LENGTH + 1];
	ASI_IMG_TYPE o_img_type;
	char o_filename[PATH_MAX + 1];
	bool o_color;
	int o_verbose;
	double o_exposure;
};

/* Default settings. */
struct options opt = {
	.o_list = false,
	.o_capa = false,
	.o_cam_id = 0,
	.o_get = {0},
	.o_set = {0},
	.o_capture = false,
	.o_width = 640,
	.o_height = 480,
	.o_binning = 1,		/* 1 x 1 */
	.o_img_type_s = {0},
	.o_img_type = 0,	/* RAW8 */
	.o_filename = {0},
	.o_color = false,
	.o_verbose = API_MSG_NORMAL,
	.o_exposure = 0.01,	/* 0.01 sec */
};

static struct params_vals pvs = {.N = 0,
				 .pv = NULL};

struct fit_head_s {
	char date_obs[MAX_LEN_ISO8601];
	double exp_time;
	unsigned int x_binning;
	unsigned int y_binning;
	float pix_size1;
	float pix_size2;
};

static void tiff_error_handler(const char *module, const char *fmt, va_list ap)
{
	fprintf(stderr, RED "[ERROR] " RESET "%f [%ld] %s"
		" %s ", c_now(), syscall(SYS_gettid), __FILE__, module);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr,"\n");
}

static void tiff_warn_handler(const char *module, const char *fmt, va_list ap)
{
	if (opt.o_verbose < API_MSG_WARN)
		return;

	fprintf(stderr, RED "[WARN] " RESET "%f [%ld] %s"
		" %s ", c_now(), syscall(SYS_gettid), __FILE__, module);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr,"\n");
}

static void usage(const char *cmd_name, const int rc)
{
	fprintf(stdout, "usage: %s [options] <camera_id>\n"
		"\t-l, --list\t\t\t\t list properties of connected cameras\n"
		"\t-p, --capabilities <camera_id>\t\t list capabilities and values\n"
		"\t-s, --set <param=val> <camera_id>\t set value of parameter name\n"
		"\t-g, --get <param> <camera_id>\t\t get value of parameter name\n"
		"\t-c, --capture <camera_id>\t\t start single image capture\n"
		"\t-e, --exposure <double>\t\t\t set exposure time in seconds [default: %.2f]\n"
		"\t-w, --width <int>\t\t\t image width [default: %d]\n"
		"\t-h, --height <int>\t\t\t image height [default: %d]\n"
		"\t-b, --binning <int>\t\t\t pixel binning [default: %d]\n"
		"\t-t, --type <string>\t\t\t image type {RAW8, RAW16, RGB24, Y8} [default: %s]\n"
		"\t-f, --filename <string>\t\t\t tif or fit filename of captured data\n"
		"\t-v, --verbose {error, warn, message, info, debug} [default: message]\n"
		"version: %s (%s) © by Thomas Stibor <thomas@stibor.net>\n",
		cmd_name, opt.o_exposure,
		opt.o_width, opt.o_height,
		opt.o_binning, IMG_TYPE[opt.o_img_type],
		PACKAGE_VERSION, __DATE__);
	exit(rc);
}

static void set_img_outtype(const char *filename)
{
	img_outtype = TYPE_UNKNOWN;

	if (!filename)
		return;

	char *s = rindex(filename, '.');

	if (!s || strlen(s) < 4)
		return;

	char *p = s + 1;

	for ( ; *p; ++p)
		*p = tolower(*p);

	if (STRNCMP(s + 1, "tif") || STRNCMP(s + 1, "tiff"))
		img_outtype = TYPE_TIF;
	else if (STRNCMP(s + 1, "fit") || STRNCMP(s + 1, "fits"))
		img_outtype = TYPE_FIT;
}

static void sanity_arg_check(const char *argv)
{
	if (opt.o_capture) {
		if (!strlen(opt.o_filename)) {
			fprintf(stdout, "missing output filename\n");
			usage(argv, 1);
		}
		if (img_outtype == TYPE_UNKNOWN) {
			fprintf(stdout, "unkown image output type filename, "
				"valid types are <filename>.fit or "
				"<filename>.tif\n");
			usage(argv, 1);
		}
	}
}

static int parseopts(int argc, char *argv[])
{
	struct option long_opts[] = {
		{"list",         no_argument,       0, 'l'},
		{"capabilities", no_argument,       0, 'p'},
		{"set",          required_argument, 0, 's'},
		{"get",          required_argument, 0, 'g'},
		{"capture",      no_argument,       0, 'c'},
		{"exposure",     required_argument, 0, 'e'},
		{"width",        required_argument, 0, 'w'},
		{"height",       required_argument, 0, 'h'},
		{"binning",      required_argument, 0, 'b'},
		{"type",         required_argument, 0, 't'},
		{"filename",     required_argument, 0, 'f'},
		{"verbose",	 required_argument, 0, 'v'},
		{0, 0, 0, 0}
	};

	int c;
	while ((c = getopt_long(argc, argv, "lpg:s:ce:w:h:b:t:f:v:",
				long_opts, NULL)) != -1) {
		switch (c) {
		case 'l': {
			opt.o_list = true;
			break;
		}
		case 'p': {
			opt.o_capa = true;
			break;
		}
		case 's': {
			strncpy(opt.o_set, optarg, MAX_PV_SET_LENGTH);
			break;
		}
		case 'g': {
			strncpy(opt.o_get, optarg, MAX_PV_LENGTH);
			break;
		}
		case 'c': {
			opt.o_capture = true;
			break;
		}
		case 'e': {
			opt.o_exposure = atof(optarg);
			break;
		}
		case 'w': {
			opt.o_width = atoi(optarg);
			break;
		}
		case 'h': {
			opt.o_height = atoi(optarg);
			break;
		}
		case 'b': {
			opt.o_binning = atoi(optarg);
			break;
		}
		case 't': {
			if (STRNCMP("RAW8", optarg))
				opt.o_img_type = ASI_IMG_RAW8;
			else if (STRNCMP("RAW16", optarg))
				opt.o_img_type = ASI_IMG_RAW16;
			else if (STRNCMP("RGB24", optarg))
				opt.o_img_type = ASI_IMG_RGB24;
			else if (STRNCMP("Y8", optarg))
				opt.o_img_type = ASI_IMG_Y8;
			else {
				fprintf(stderr, "unknown image type parameter: %s\n", optarg);
				usage(argv[0], 1);
			}
			break;
		}
		case 'f': {
			strncpy(opt.o_filename, optarg, PATH_MAX);
			set_img_outtype(opt.o_filename);
			break;
		}
		case 'v': {
			if (STRNCMP("error", optarg))
				opt.o_verbose = API_MSG_ERROR;
			else if (STRNCMP("warn", optarg))
				opt.o_verbose = API_MSG_WARN;
			else if (STRNCMP("message", optarg))
				opt.o_verbose = API_MSG_NORMAL;
			else if (STRNCMP("info", optarg))
				opt.o_verbose = API_MSG_INFO;
			else if (STRNCMP("debug", optarg))
				opt.o_verbose = API_MSG_DEBUG;
			else {
				fprintf(stdout, "wrong argument for -v, "
					"--verbose '%s'\n", optarg);
				usage(argv[0], 1);
			}
			api_msg_set_level(opt.o_verbose);
			break;
		}
		case 0: {
			break;
		}
		default:
			return -EINVAL;
		}
	}

	sanity_arg_check(argv[0]);

	return 0;
}

int write_fit(uint8_t *img_buf, const struct fit_head_s *fit_head)
{
	int rc = 0;
	int status = 0;
	fitsfile *fitfile = NULL;
	const long naxis = 2;
	long naxes[2] = {opt.o_width, opt.o_height};
	const long size = naxes[0] * naxes[1];
	int bitpix;

	switch (opt.o_img_type) {
	case ASI_IMG_RAW8: {
		bitpix = BYTE_IMG;
		break;
	}
	case ASI_IMG_RAW16: {
		bitpix = USHORT_IMG;
		break;
	}
	default:
		rc = -EINVAL;
		C_ERROR(rc, "unsupported ASI image type '%s' for fit format",
			IMG_TYPE[opt.o_img_type]);
		return rc;
	}

	status = 0;
	fits_create_file(&fitfile, opt.o_filename, &status);
	if (status) {
		FITS_ERROR(status);
		return -EPERM;
	}

	status = 0;
	fits_create_img(fitfile, bitpix, naxis, naxes, &status);
	if (status) {
		rc = -EPERM;
		FITS_ERROR(status);
		goto cleanup;
	}

	status = 0;
	fits_write_img(fitfile, bitpix == BYTE_IMG ? TBYTE : USHORT_IMG,
		       1, size, img_buf, &status);
	if (status) {
		rc = -EPERM;
		FITS_ERROR(status);
		goto cleanup;
	}

	status = 0;
	fits_update_key(fitfile, TSTRING, "DATE-OBS", (char *)fit_head->date_obs,
			"UTC of exposure start", &status);
	fits_update_key(fitfile, TDOUBLE, "EXPTIME", (double *)&fit_head->exp_time,
			"Exposure time (seconds)", &status);

	fits_update_key(fitfile, TUINT, "XBINNING",
			(unsigned int *)&fit_head->x_binning,
			"Binning factor in width", &status);
	fits_update_key(fitfile, TUINT, "YBINNING",
			(unsigned int *)&fit_head->y_binning,
			"Binning factor in height", &status);
	fits_update_key(fitfile, TFLOAT,
			"PIXSIZE1", (float *)&fit_head->pix_size1,
			"Pixel Size 1 (microns)", &status);
	fits_update_key(fitfile, TFLOAT,
			"PIXSIZE2", (float *)&fit_head->pix_size1,
			"Pixel Size 2 (microns)", &status);

	char str[64] = {0};
	snprintf(str, 64, "Generated by %s version %s", "asic",
		 PACKAGE_VERSION);
	fits_write_comment(fitfile, str, &status);
	fits_write_comment(fitfile, "See: https://github.com/tstibor/asic", &status);

	if (status)
		FITS_ERROR(status);

cleanup:
	status = 0;
	fits_close_file(fitfile, &status);
	if (status) {
		FITS_ERROR(status);
		rc = -EPERM;
	}

	if (!rc)
		C_MESSAGE("created successfully '%s'", opt.o_filename);

	return rc;
}

static void set_tiff_fields(TIFF *tiff_img, int8_t bps, int8_t spp)
{
	const time_t _time = time(NULL);
	char time_str[24 + 1] = {0};
	strftime(time_str, 24, "%Y:%m:%d %H:%M:%S", localtime(&_time));

	/* Write TIFF tags. */
	TIFFSetField(tiff_img, TIFFTAG_DATETIME, time_str);
	TIFFSetField(tiff_img, TIFFTAG_IMAGEWIDTH, opt.o_width);
	TIFFSetField(tiff_img, TIFFTAG_IMAGELENGTH, opt.o_height);
	TIFFSetField(tiff_img, TIFFTAG_BITSPERSAMPLE, bps);
	TIFFSetField(tiff_img, TIFFTAG_SAMPLESPERPIXEL, spp);
	TIFFSetField(tiff_img, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
	TIFFSetField(tiff_img, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(tiff_img, TIFFTAG_PHOTOMETRIC, is_color(opt.o_img_type) ?
		     PHOTOMETRIC_RGB : PHOTOMETRIC_MINISBLACK);
#if 0
	TIFFSetField(tiff_img, TIFFTAG_ROWSPERSTRIP,
		     TIFFDefaultStripSize(tiff_img, opt.o_width * spp));
#endif

	/* Write EXIF tags. */
	uint64 exif_dir_offset = 0;
	TIFFSetField(tiff_img, TIFFTAG_EXIFIFD, exif_dir_offset);
	TIFFCheckpointDirectory(tiff_img);
	TIFFSetDirectory(tiff_img, 0);
	TIFFCreateEXIFDirectory(tiff_img);
	TIFFSetField(tiff_img, EXIFTAG_EXPOSURETIME, opt.o_exposure);
	TIFFWriteCustomDirectory(tiff_img, &exif_dir_offset);
	TIFFSetDirectory(tiff_img, 0);
	TIFFSetField(tiff_img, TIFFTAG_EXIFIFD, exif_dir_offset);
	TIFFCheckpointDirectory(tiff_img);
	TIFFWriteDirectory(tiff_img);
	TIFFSetDirectory(tiff_img, 0);

}

int write_tiff(TIFF *tiff_img, uint8_t *img_buf, int8_t bps, int8_t spp)
{
	int rc = 0;

	tiff_img = TIFFOpen(opt.o_filename, "w");
	if (!tiff_img)
		/* Error message handled by tiff_error_handler */
		return -ECANCELED;

	set_tiff_fields(tiff_img, bps, spp);
	const uint16_t factor = opt.o_width * bits_per_sample(opt.o_img_type) / 8;

	for (int y = 0; y < opt.o_height; ++y) {
		rc = TIFFWriteScanline(tiff_img, img_buf + y * factor, y, 0);
		if (rc == -1) {
			rc = -ECANCELED;
			/* Error message handled by tiff_error_handler */
			break;
		}
	}

	TIFFClose(tiff_img);
	if (rc == -ECANCELED)
		C_ERROR(rc, "tiff image creation failed");
	else {
		C_MESSAGE("created successfully '%s'", opt.o_filename);
		rc = 0;
	}

	return rc;
}

static void list_devices(const int n_devices)
{
	ASI_CAMERA_INFO ASI_camera_info;
	for (int i = 0; i < n_devices; ++i) {
		ASIGetCameraProperty(&ASI_camera_info, i);
		fprintf(stdout, "camera id     : %d\n", ASI_camera_info.CameraID);
		fprintf(stdout, "name          : %s\n", ASI_camera_info.Name);
		fprintf(stdout, "max width     : %ld\n", ASI_camera_info.MaxWidth);
		fprintf(stdout, "max height    : %ld\n", ASI_camera_info.MaxHeight);
		fprintf(stdout, "color         : %s\n", BOOL_NO_YES[ASI_camera_info.IsColorCam]);
		if (ASI_camera_info.IsColorCam)
			fprintf(stdout, "bayer pattern : %s\n",
				BAYER_PATTERN[ASI_camera_info.BayerPattern]);
		fprintf(stdout, "pixel size    : %.3f\n", ASI_camera_info.PixelSize);
		fprintf(stdout, "mech. shutter : %s\n", BOOL_NO_YES[ASI_camera_info.MechanicalShutter]);
		fprintf(stdout, "st4 port      : %s\n", BOOL_NO_YES[ASI_camera_info.ST4Port]);
		fprintf(stdout, "e / ADU       : %.3f\n", ASI_camera_info.ElecPerADU);
		fprintf(stdout, "image type    : ");
		ASI_IMG_TYPE *it = ASI_camera_info.SupportedVideoFormat;
		while (*it != -1)
			fprintf(stdout, "%s ", IMG_TYPE[*it++]);
		fprintf(stdout, "\nbinning       : ");
		int *ib = ASI_camera_info.SupportedBins;
		while (*ib != 0) {
			fprintf(stdout, "(%d x %d) ", *ib, *ib);
			ib++;
		}
		fprintf(stdout, "\ncooling       : %s\n", BOOL_NO_YES[ASI_camera_info.IsCoolerCam]);
		fprintf(stdout, "usb3 camera   : %s\n", BOOL_NO_YES[ASI_camera_info.IsUSB3Camera]);
		fprintf(stdout, "usb3 host     : %s\n", BOOL_NO_YES[ASI_camera_info.IsUSB3Host]);
	}
}

static void list_ctrl_caps(const int cam_id, const int n_ctrl)
{
	ASI_CONTROL_CAPS ctrl_caps;
	const char *format = "| %-24s| %-50s| %-15s| %-15s| %-14s| %-17s| %-8s |\n";
	const uint8_t len = 16;
	char max_val[len + 1];
	char min_val[len + 1];
	char def_val[len + 1];

	bzero(max_val, len + 1);
	bzero(min_val, len + 1);
	bzero(def_val, len + 1);

	fprintf(stdout, format,
		"name", "description", "max value", "min value", "default value", "support auto set", "writable");
	fprintf(stdout, "|%-24s+%-50s+%-15s+%-15s+%-14s+%-17s+%-8s|\n",
		"-------------------------",
		"---------------------------------------------------",
		"----------------",
		"----------------",
		"---------------",
		"------------------",
		"----------");

	for (int i = 0; i < n_ctrl; ++i) {
		int rc;
		rc = ASIGetControlCaps(cam_id, i, &ctrl_caps);
		C_DEBUG("[rc:%d, id:%d] ASIGetControlCaps", rc, cam_id);
		if (rc) {
			ASI_C_ERROR(rc, "ASIGetControlCaps");
			return;
		}

		snprintf(max_val, len, "%ld", ctrl_caps.MaxValue);
		snprintf(min_val, len, "%ld", ctrl_caps.MinValue);
		snprintf(def_val, len, "%ld", ctrl_caps.DefaultValue);

		fprintf(stdout, format,
			ctrl_caps.Name,
			ctrl_caps.Description,
			max_val,
			min_val,
			def_val,
			BOOL_NO_YES[ctrl_caps.IsAutoSupported],
			BOOL_NO_YES[ctrl_caps.IsWritable]);
	}
}

static int split_pvs(char *str, struct params_vals *pvs)
{
	if (!pvs)
		return -EINVAL;

	const char *delim = ", ";
	char *token = NULL;

	token = strtok(str, delim);
	while (token != NULL) {
		char *p = strstr(token, "=");
		if (!p)
			return -EINVAL;

		pvs->pv = realloc(pvs->pv, sizeof(struct param_val) * (pvs->N + 1));
		if (!pvs->pv)
			return -EINVAL;

		bzero(pvs->pv[pvs->N].param, MAX_PV_LENGTH + 1);
		bzero(pvs->pv[pvs->N].val, MAX_PV_LENGTH + 1);

		strncpy(pvs->pv[pvs->N].param, token, strlen(token) - strlen(p));
		strncpy(pvs->pv[pvs->N].val, p + 1, strlen(p) - 1);
		pvs->N++;
		token = strtok(NULL, delim);
	}

	return 0;
}

static void capture(struct options opt)
{
	int rc;
	uint8_t *img_buf = NULL;

	rc = ASISetROIFormat(opt.o_cam_id, opt.o_width, opt.o_height, opt.o_binning, opt.o_img_type);
	C_DEBUG("[rc:%d, id:%d, width:%d, height:%d, type:%s] ASISetROIFormat",
		rc, opt.o_cam_id, opt.o_width, opt.o_height, IMG_TYPE[opt.o_img_type]);
	if (rc) {
		ASI_C_ERROR(rc, "ASISetROIFormat");
		return;
	}

	rc = ASIGetROIFormat(opt.o_cam_id, &opt.o_width, &opt.o_height, &opt.o_binning, &opt.o_img_type);
	C_DEBUG("[rc:%d, id:%d, width:%d, height:%d, binning:%dx%d, type:%s] "
		"ASIGetROIFormat", rc, opt.o_cam_id, opt.o_width, opt.o_height,
		opt.o_binning, opt.o_binning, IMG_TYPE[opt.o_img_type]);
	if (rc) {
		ASI_C_ERROR(rc, "ASIGetROIFormat");
		return;
	}

	const long size = calc_buf_size(opt.o_width, opt.o_height, opt.o_img_type);
	if (size < 0) {
		C_ERROR(EINVAL, "calc_buf_size");
		return;
	}

	int8_t bps = bits_per_sample(opt.o_img_type);
	C_DEBUG("[bps:%d] bits_per_sample", bps);
	if (bps < 0) {
		C_ERROR(EINVAL, "bits_per_sample");
		return;
	}

	int8_t spp = samples_per_pixel(opt.o_img_type);
	C_DEBUG("[spp:%d] samples_per_pixel", spp);
	if (bps < 0) {
		C_ERROR(EINVAL, "samples_per_pixel");
		return;
	}

	img_buf = calloc(size, sizeof(uint8_t));
	if (!img_buf) {
		C_ERROR(errno, "calloc");
		return;
	}

	/* For whatever reason, sometimes the exposure fails for exposure time
	 > 0.5 sec. However by first creating a very short exposure and subsequently the
	 desired exposure, the ASI library seems to be working more reliably. */
	C_MESSAGE("capture image %d x %d, exposure (sec): %5.25f, "
		  "binning: %d x %d, type: %s, size (bytes): %ld",
		  opt.o_width , opt.o_height, opt.o_exposure,
		  opt.o_binning, opt.o_binning,
		  IMG_TYPE[opt.o_img_type], size);

	uint8_t cur_attempt = 1;
	uint8_t max_attempt = 3;
	ASI_EXPOSURE_STATUS status = ASI_EXP_WORKING;

	ASI_CAMERA_INFO ASI_camera_info;
	ASIGetCameraProperty(&ASI_camera_info, opt.o_cam_id);

	struct fit_head_s fit_head = {
		.date_obs = {0},
		.exp_time = opt.o_exposure,
		.x_binning = opt.o_binning,
		.y_binning = opt.o_binning,
		.pix_size1 = ASI_camera_info.PixelSize,
		.pix_size2 = ASI_camera_info.PixelSize
	};

	/* Setup DATE-OBS field as current date/time UTC. */
	time_t cur_time = time(NULL);
	strftime(fit_head.date_obs, MAX_LEN_ISO8601 * sizeof(char),
		 "%Y-%m-%dT%H:%M:%S", gmtime(&cur_time));

	while (1) {
		rc = ASIStartExposure(opt.o_cam_id, ASI_FALSE);
		C_DEBUG("[rc:%d, id:%d] ASIStartExposure", rc, opt.o_cam_id);
		if (rc) {
			ASI_C_ERROR(rc, "ASIStartExposure");
			goto cleanup;
		}
		usleep(10000);	/* 10ms. */
		while (status == ASI_EXP_WORKING) {
			rc = ASIGetExpStatus(opt.o_cam_id, &status);
			C_DEBUG("[rc:%d, id:%d] ASIGetExpStatus, status: %s",
				rc, opt.o_cam_id, ASI_EXP_STATUS_MSG(status));
			if (rc) {
				ASI_C_ERROR(rc, "ASIGetExpStatus");
				goto cleanup;
			}
		}

		if (status == ASI_EXP_SUCCESS) {
			C_MESSAGE("%s", ASI_EXP_STATUS_MSG(status));
			break;

		} else if (status == ASI_EXP_FAILED) {
			if (cur_attempt == max_attempt) {
				C_ERROR(ECANCELED, "ASIGetExpStatus %s",
					ASI_EXP_STATUS_MSG(status));
				goto cleanup;
			}
			C_WARN("ASIGetExpStatus %s. Restarting exposure attempt %d.",
			       ASI_EXP_STATUS_MSG(status), cur_attempt);
			cur_attempt++;
		} else {	/* We should never be in this state (ASI_EXP_IDLE). */
			ASI_C_ERROR(ASI_ERROR_TIMEOUT, "invalid exposure state");
			goto cleanup;
		}
	}

	rc = ASIGetDataAfterExp(opt.o_cam_id, img_buf, size);
	C_DEBUG("[rc:%d, id:%d] ASIGetDataAfterExp", rc, opt.o_cam_id);
	if (rc) {
		ASI_C_ERROR(rc, "ASIGetDataAfterExp");
		goto cleanup;
	}

	if (img_outtype == TYPE_TIF) {
		TIFF *tiff_img = NULL;
		rc = write_tiff(tiff_img, img_buf, bps, spp);
		if (rc)
			C_ERROR(rc, "write_tiff");
	} else if (img_outtype == TYPE_FIT) {
		rc = write_fit(img_buf, &fit_head);
		if (rc)
			C_ERROR(rc, "write_fit");
	} else
		C_ERROR(-EINVAL, "unknown image type");

cleanup:
	if (img_buf) {
		free(img_buf);
		img_buf = NULL;
	}

	rc = ASIStopExposure(opt.o_cam_id);
	C_DEBUG("[rc:%d, id:%d] ASIStopExposure", rc, opt.o_cam_id);
	if (rc)
		ASI_C_ERROR(rc, "ASIStopExposure");
}

static int set_defaults(struct options opt)
{
	int rc;
	long val = opt.o_exposure * (long)1e6;

	rc = ASISetControlValue(opt.o_cam_id, ASI_EXPOSURE, val, ASI_FALSE);
	C_DEBUG("[rc:%d, id:%d] ASISetControlValue", rc, opt.o_cam_id);
	if (rc)
		ASI_C_ERROR(rc, "ASISetControlValue");

	return rc;
}

int main(int argc, char *argv[])
{
	if (argc == 1)
		usage(argv[0], 1);

	api_msg_set_level(opt.o_verbose);
	TIFFSetErrorHandler(&tiff_error_handler);
	TIFFSetWarningHandler(&tiff_warn_handler);

	int rc;
	rc = parseopts(argc, argv);
	if (rc) {
		fprintf(stdout, "try '%s --help' for more information\n", argv[0]);
		return 1;
	}

	if (argc > optind)
		opt.o_cam_id = atoi(argv[optind]);

	int devs_id = 0;
	devs_id = ASIGetNumOfConnectedCameras();
	C_DEBUG("[devs_id:%d] ASIGetNumOfConnectedCameras", devs_id);
	if (devs_id <= 0) {
		rc = ASI_ERROR_INVALID_INDEX;
		ASI_C_ERROR(rc, "ASIGetNumOfConnectedCameras");
		return rc;
	}

	if (opt.o_list)
		list_devices(devs_id);

	rc = ASIOpenCamera(opt.o_cam_id);
	C_DEBUG("[rc:%d, id:%d] ASIOpenCamera", rc, opt.o_cam_id);
	if (rc) {
		ASI_C_ERROR(rc, "ASIOpenCamera");
		goto cleanup;
	}

	rc = ASIInitCamera(opt.o_cam_id);
	C_DEBUG("[rc:%d, id:%d] ASIInitCamera", rc, opt.o_cam_id);
	if (rc) {
		ASI_C_ERROR(rc, "ASIInitCamera");
		goto cleanup;
	}

	rc = set_defaults(opt);
	if (rc)
		goto cleanup;

	if (opt.o_capa) {
		int n_ctrl = 0;
		rc = ASIGetNumOfControls(opt.o_cam_id, &n_ctrl);
		if (rc) {
			ASI_C_ERROR(rc, "ASIGetNumOfControls");
			goto cleanup;
		}
		list_ctrl_caps(opt.o_cam_id, n_ctrl);
	}
	if (strlen(opt.o_set)) {
		rc = split_pvs(opt.o_set, &pvs);
		if (rc) {
			C_ERROR(rc, "split_pvs");
			goto cleanup;
		}

		for (uint8_t n = 0; n < pvs.N; n++) {
			int ctrl_type;
			ASI_BOOL set_auto = ASI_FALSE;
			long val;
			fprintf(stdout, "%s %s\n",
				pvs.pv[n].param,
				pvs.pv[n].val);

			ctrl_type = lookup_ctrl_type(pvs.pv[n].param);

			if (strlen(pvs.pv[n].val) == 4 && !strncmp("auto", pvs.pv[n].val, 4)) {
				/* Get previous value and set again previous value and set_auto (TRUE). */
				rc = ASIGetControlValue(opt.o_cam_id, ctrl_type, &val, &set_auto);
				C_DEBUG("[rc:%d, id:%d] ASIGetControlValue", rc, opt.o_cam_id);
				if (rc) {
					ASI_C_ERROR(rc, "ASIGetControlValue");
					goto cleanup;
				}
				set_auto = ASI_TRUE;
			} else
				val = atol(pvs.pv[n].val);

			rc = ASISetControlValue(opt.o_cam_id, ctrl_type, val, set_auto);
			C_DEBUG("[rc:%d, id:%d] ASISetControlValue", rc, opt.o_cam_id);
			if (rc)
				ASI_C_ERROR(rc, "ASISetControlValue");
		}
	}
	if (opt.o_exposure > 0) {
		long val = opt.o_exposure * 1e6;
		rc = ASISetControlValue(opt.o_cam_id, ASI_EXPOSURE, val, ASI_FALSE);
		C_DEBUG("[rc:%d, id:%d] ASISetControlValue", rc, opt.o_cam_id);
		if (rc) {
			ASI_C_ERROR(rc, "ASISetControlValue");
			goto cleanup;
		}
	}
	if (strlen(opt.o_get)) {
		int ctrl_type;
		long val = 0;
		ASI_BOOL asi_bool;
		ctrl_type = lookup_ctrl_type(opt.o_get);
		rc = ASIGetControlValue(opt.o_cam_id, ctrl_type, &val, &asi_bool);
		C_DEBUG("[rc:%d, id:%d] ASIGetControlValue", rc, opt.o_cam_id);
		if (rc) {
			ASI_C_ERROR(rc, "ASIGetControlValue");
			goto cleanup;
		}
		fprintf(stdout, "%s %ld %s\n", opt.o_get, val, BOOL_STR[asi_bool]);
	}
	if (opt.o_capture)
		capture(opt);

cleanup:
	rc = ASICloseCamera(opt.o_cam_id);
	C_DEBUG("[rc:%d, id:%d] ASICloseCamera", rc, opt.o_cam_id);
	if (rc)
		ASI_C_ERROR(rc, "ASICloseCamera");

	if (pvs.pv) {
		free(pvs.pv);
		pvs.pv = NULL;
		pvs.N = 0;
	}

	return rc;
}
