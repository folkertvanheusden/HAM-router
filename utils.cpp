// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string.h>


void *duplicate(const void *in, const size_t size)
{
	void *out = malloc(size);
	if (!out)
		return nullptr;

	memcpy(out, reinterpret_cast<const uint8_t *>(in), size);

	return out;
}

void set_thread_name(const std::string & name)
{
	std::string full_name = "A25:" + name;

	if (full_name.length() > 15)
		full_name = full_name.substr(0, 15);

	pthread_setname_np(pthread_self(), full_name.c_str());
}
