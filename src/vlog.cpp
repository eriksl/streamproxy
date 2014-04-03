#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>

char foreground = 1;

void vlog(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	if(foreground)
	{
		vfprintf(stderr, format, ap);

		if(errno)
			fprintf(stderr, ", system error: %s", strerror(errno));

		fputs("\n", stderr);
	}
	else
	{
		vsyslog(LOG_WARNING, format, ap);

		if(errno)
			syslog(LOG_WARNING, "system error: %s\n", strerror(errno));
	}

	errno = 0;
	va_end(ap);
}
