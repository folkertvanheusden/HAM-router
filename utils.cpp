// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


void *duplicate(const void *in, const size_t size)
{
	void *out = malloc(size);
	if (!out)
		return nullptr;

	memcpy(out, reinterpret_cast<const uint8_t *>(in), size);

	return out;
}
