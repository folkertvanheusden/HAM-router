#include <atomic>
#include <stdint.h>

uint64_t get_us();

void     myusleep(unsigned duration, std::atomic_bool *const terminate);
