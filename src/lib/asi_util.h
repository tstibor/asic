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

#ifndef ASI_UTIL_H
#define ASI_UTIL_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <ASICamera2.h>

#define MAX_PV_LENGTH 64
#define MAX_PV_SET_LENGTH 512

#define STRNCMP(str1, str2)				\
	((strlen(str1) == strlen(str2)) &&		\
	 (strncmp(str1, str2, strlen(str1)) == 0))

#define ASI_C_ERROR(_rc, _format, ...)				     \
	_asilog(_rc,						     \
	    RED "[ERROR] " RESET "%f [%ld] %s:%d "_format,	     \
	    c_now(), syscall(SYS_gettid), __FILE__, __LINE__,	     \
	    ## __VA_ARGS__)

const char *ASI_EXP_STATUS_MSG(const ASI_EXPOSURE_STATUS asi_exp_status);
const char *ASI_ERR_CODE_MSG(const ASI_ERROR_CODE asi_err_code);
int lookup_ctrl_type(char *param);
long calc_buf_size(const int width, const int height, const ASI_IMG_TYPE asi_img_type);
int8_t bits_per_sample(const ASI_IMG_TYPE asi_img_type);
int8_t samples_per_pixel(const ASI_IMG_TYPE asi_img_type);
bool is_color(const ASI_IMG_TYPE asi_img_type);
void _asilog(int err, const char *fmt, ...);

#endif	/* ASI_UTIL_H */
