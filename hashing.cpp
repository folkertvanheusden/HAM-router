#include <stdint.h>

extern "C" {
#include "crc_32.h"
}


uint32_t calc_crc32(const uint8_t *const p, const size_t len)
{
	return crc32buf(reinterpret_cast<char *>(const_cast<uint8_t *>(p)), len);
}
