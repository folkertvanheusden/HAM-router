#include <assert.h>
#include <atomic>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "time.h"


uint64_t get_us()
{
	struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);

	return uint64_t(tv.tv_sec) * uint64_t(1000 * 1000) + uint64_t(tv.tv_nsec / 1000);
}

double get_us_float()
{
	return get_us() / 1000000.0;
}

bool myusleep(const uint64_t duration, std::atomic_bool *const terminate)
{
	constexpr unsigned check_interval = END_CHECK_INTERVAL_us;
	uint64_t           left           = duration;

	while(left > 0 && *terminate == false) {
		if (left < check_interval) {
			usleep(left);

			break;
		}

		usleep(check_interval);

		left -= check_interval;

		assert(left < duration);
	}

	return *terminate == false;
}

std::chrono::system_clock::time_point to_time_point(const timeval & tv)
{
    return std::chrono::system_clock::time_point{std::chrono::seconds{tv.tv_sec} + std::chrono::microseconds{tv.tv_usec}};
}
