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

#include "asi_util.h"

static void __asilog(int err, const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
	if (!err)
		fprintf(stderr, "\n");
	else
		fprintf(stderr, ": %s (%d)\n", ASI_ERR_CODE_MSG(err), err);
}

void _asilog(int err, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	__asilog(abs(err), fmt, args);
        va_end(args);
}

const char *ASI_EXP_STATUS_MSG(const ASI_EXPOSURE_STATUS asi_exp_status)
{
	switch (asi_exp_status) {
	case ASI_EXP_IDLE:
		return "idle states, you can start to exposure now";
	case ASI_EXP_WORKING:
		return "exposing";
	case ASI_EXP_SUCCESS:
		return "exposure finished and waiting for download";
	case ASI_EXP_FAILED:
		return "exposure failed, you need to start exposure again";
	default:
		return "unknown exposure status";
	}
	return "unknown exposure status";
}

const char *ASI_ERR_CODE_MSG(const ASI_ERROR_CODE asi_err_code)
{
	switch (asi_err_code) {
	case ASI_ERROR_INVALID_INDEX:
		return "no camera connected or index value out of boundary";
	case ASI_ERROR_INVALID_ID:
		return "invalid camera id";
	case ASI_ERROR_INVALID_CONTROL_TYPE:
		return "invalid control type";
	case ASI_ERROR_CAMERA_CLOSED:
		return "camera did not open";
	case ASI_ERROR_CAMERA_REMOVED:
		return "failed to find the camera, maybe the camera has been removed";
	case ASI_ERROR_INVALID_PATH:
		return "cannot find the path of the file";
	case ASI_ERROR_INVALID_FILEFORMAT:
		return "invalid file format";
	case ASI_ERROR_INVALID_SIZE:
		return "wrong video format size";
	case ASI_ERROR_INVALID_IMGTYPE:
		return "unsupported image format";
	case ASI_ERROR_OUTOF_BOUNDARY:
		return "start position is out of boundary";
	case ASI_ERROR_TIMEOUT:
		return "camera timeout";
	case ASI_ERROR_INVALID_SEQUENCE:
		return "stop capture first";
	case ASI_ERROR_BUFFER_TOO_SMALL:
		return "buffer size is too small";
	case ASI_ERROR_VIDEO_MODE_ACTIVE:
		return "video mode is active";
	case ASI_ERROR_EXPOSURE_IN_PROGRESS:
		return "exposure in progress";
	case ASI_ERROR_GENERAL_ERROR:
		return "general error, e.g, value is out of valid range";
	default:
		return "unknown error code";
	}
	return "unkown error code";
}

int lookup_ctrl_type(char *param)
{
	if (!param)
		return -EINVAL;

	char *p = param;
	for ( ; *p; ++p)
		*p = tolower(*p);

	if (STRNCMP(param, "gain"))
		return ASI_GAIN;
	else if (STRNCMP(param, "exposure"))
		return ASI_EXPOSURE;
	else if (STRNCMP(param, "gamma"))
		return ASI_GAMMA;
	else if (STRNCMP(param, "flip"))
		return ASI_FLIP;
	else if (STRNCMP(param, "highspeedmode"))
		return ASI_HIGH_SPEED_MODE;
	else if (STRNCMP(param, "overclock"))
		return ASI_OVERCLOCK;
	else if (STRNCMP(param, "brightness"))
		return ASI_BRIGHTNESS;
	else if (STRNCMP(param, "bandwidthoverload"))
		return ASI_BANDWIDTHOVERLOAD;
	else if (STRNCMP(param, "temperature"))
		return ASI_TEMPERATURE;
	else if (STRNCMP(param, "automaxgain"))
		return ASI_AUTO_MAX_GAIN;
	else if (STRNCMP(param, "automaxexp"))
		return ASI_AUTO_MAX_EXP;
	else if (STRNCMP(param, "automaxbrightness"))
		return ASI_AUTO_MAX_BRIGHTNESS;
	else if (STRNCMP(param, "fanon"))
		return ASI_FAN_ON;
	else if (STRNCMP(param, "hardwarebin"))
		return ASI_HARDWARE_BIN;
	else if (STRNCMP(param, "cooleron"))
		return ASI_COOLER_ON;
	else if (STRNCMP(param, "targettemp"))
		return ASI_TARGET_TEMP;
	else
		return -EINVAL;
}

long calc_buf_size(const int width, const int height, const ASI_IMG_TYPE asi_img_type)
{
	/* Size in bytes is calculated as:
	   8bit  mono width * height
	   16bit mono width * height * 2
	   RGB24      width * height * 3 */
	long size = -1;

	switch (asi_img_type) {
	case ASI_IMG_RAW8:	/* Each pixel is an 8-bit (1 byte) gray level. */
		size = width * height;
		break;
	case ASI_IMG_RAW16:	/* 2 bytes for every pixel with 65536 gray levels. */
		size = width * height * 2;
		break;
	case ASI_IMG_RGB24:	/* Each pixel consists of RGB, 3 bytes totally (color cameras only). */
		size = width * height * 3;
		break;
	case ASI_IMG_Y8:
		size = width * height; /* Monochrome mode, 1 byte every pixel (color cameras only). */
	}

	return size;
}

int8_t bits_per_sample(const ASI_IMG_TYPE asi_img_type)
{
	int8_t bps = -1;

	switch (asi_img_type) {
	case ASI_IMG_RAW8:
		bps = 8;
		break;
	case ASI_IMG_RAW16:
		bps = 16;
		break;
	case ASI_IMG_RGB24:
		bps = 24;
		break;
	case ASI_IMG_Y8:
		bps = 8;
		break;
	}

	return bps;
}

int8_t samples_per_pixel(const ASI_IMG_TYPE asi_img_type)
{
	int8_t spp = -1;

	switch (asi_img_type) {
	case ASI_IMG_RAW8:
		spp = 1;
		break;
	case ASI_IMG_RAW16:
		spp = 1;
		break;
	case ASI_IMG_RGB24:
		spp = 3;
		break;
	case ASI_IMG_Y8:
		spp = 1;
		break;
	}

	return spp;
}

bool is_color(const ASI_IMG_TYPE asi_img_type)
{
	bool color = false;

	switch (asi_img_type) {
	case ASI_IMG_RAW8:
		color = false;
		break;
	case ASI_IMG_RAW16:
		color = false;
		break;
	case ASI_IMG_RGB24:
		color = true;
		break;
	case ASI_IMG_Y8:
		color = false;
		break;
	}

	return color;

}
