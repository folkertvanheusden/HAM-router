#include <atomic>
#include <chrono>
#include <stdint.h>
#include <time.h>


#define END_CHECK_INTERVAL_us (100000)  // in microseconds
#define END_CHECK_INTERVAL_ms (END_CHECK_INTERVAL_us / 1000)

uint64_t get_us();
double   get_us_float();

bool     myusleep(uint64_t duration, std::atomic_bool *const terminate);

std::chrono::system_clock::time_point to_time_point(const timeval & tv);

timeval  to_timeval(const std::chrono::milliseconds & tp);
