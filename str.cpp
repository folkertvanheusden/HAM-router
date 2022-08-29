#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

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

std::string dump_replace(const unsigned char *p, int len)
{
	std::string out;

	for(int i=0; i<len; i++)
		out += myformat("%c", p[i] > 32 && p[i] < 127 ? p[i] : '.');

	return out;
}

std::vector<std::string> split(std::string in, std::string splitter)
{
	std::vector<std::string> out;
	size_t splitter_size = splitter.size();

	for(;;)
	{
		size_t pos = in.find(splitter);
		if (pos == std::string::npos)
			break;

		std::string before = in.substr(0, pos);
		out.push_back(before);

		size_t bytes_left = in.size() - (pos + splitter_size);
		if (bytes_left == 0)
		{
			out.push_back("");
			return out;
		}

		in = in.substr(pos + splitter_size);
	}

	if (in.size() > 0)
		out.push_back(in);

	return out;
}
