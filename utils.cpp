// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <sys/time.h>

uint64_t get_us()
{
	struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);

	return uint64_t(tv.tv_sec) * uint64_t(1000 * 1000) + uint64_t(tv.tv_nsec / 1000);
}

std::string myformat(const char *const fmt, ...)
{
	char *buffer = nullptr;
        va_list ap;

        va_start(ap, fmt);
        if (vasprintf(&buffer, fmt, ap) == -1) {
		va_end(ap);
		return fmt;
	}
        va_end(ap);

	std::string result = buffer;
	free(buffer);

	return result;
}

std::string dump_hex(const unsigned char *p, int len)
{
	std::string out;

	for(int i=0; i<len; i++) {
		if (i)
			out += " ";

		out += myformat("%d[%c]", p[i], p[i] > 32 && p[i] < 127 ? p[i] : '.');
	}

	return out;
}
