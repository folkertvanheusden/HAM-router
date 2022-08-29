#include <assert.h>
#include <atomic>
#include <stdint.h>
#include <time.h>

uint64_t get_us()
{
	struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);

	return uint64_t(tv.tv_sec) * uint64_t(1000 * 1000) + uint64_t(tv.tv_nsec / 1000);
}

void myusleep(const unsigned duration, std::atomic_bool *const terminate)
{
	constexpr unsigned check_interval = 100000;
	unsigned           left           = duration;

	while(left > 0 && *terminate == false) {
		if (left < check_interval) {
			usleep(left);

			break;
		}

		usleep(check_interval);

		left -= check_interval;

		assert(left < duration);
	}
}
