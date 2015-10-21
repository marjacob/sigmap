#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include "utils.h"

int util_strtoi(const char *restrict str, char **restrict endptr, int base)
{
	long value = strtol(str, endptr, base);

	if (errno) {
		return 0;
	}

	if (INT_MAX < value || INT_MIN > value) {
		errno = ERANGE;
		return 0;
	}

	return (int)value;
}
