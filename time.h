#include <atomic>
#include <stdint.h>

#define END_CHECK_INTERVAL_us (100000)  // in microseconds
#define END_CHECK_INTERVAL_ms (END_CHECK_INTERVAL_us / 1000)

uint64_t get_us();

bool     myusleep(uint64_t duration, std::atomic_bool *const terminate);
