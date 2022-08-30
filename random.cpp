#include <assert.h>
#include <stdint.h>
#include <sys/random.h>


uint64_t get_random_uint64_t()
{
	uint64_t out { 0 };

	ssize_t  rc = getrandom(&out, sizeof out, 0);

	assert(rc == sizeof out);

	return out;
}
