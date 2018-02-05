#include "log.h"

static unsigned int api_msg_level = API_MSG_MAX;

int api_msg_get_level(void)
{
	return api_msg_level;
}

void api_msg_set_level(int level)
{
        /* ensure level is in the good range */
        if (level < API_MSG_OFF)
                api_msg_level = API_MSG_OFF;
        else if (level > API_MSG_MAX)
                api_msg_level = API_MSG_MAX;
        else
                api_msg_level = level;
}

double c_now(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec + 0.000001 * tv.tv_usec;
}

void __clog(enum api_message_level level, int err, const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
	if (level & API_MSG_NO_ERRNO || !err)
		fprintf(stderr, "\n");
	else
		fprintf(stderr, ": %s (%d)\n", strerror(err), err);
}

void _clog(enum api_message_level level, int err, const char *fmt, ...)
{
        if ((level & API_MSG_MASK) > api_msg_level)
                return;

	int tmp_errno = errno;
	va_list args;
	va_start(args, fmt);
	__clog(level, abs(err), fmt, args);
        va_end(args);
	errno = tmp_errno;
}
