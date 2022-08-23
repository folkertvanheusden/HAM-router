#include <mutex>
#include <stdint.h>
#include <string>

typedef struct {
	std::mutex  lock;
	std::string json_data;
	uint64_t    ts;
} ws_global_context_t;

void start_websocket_thread(const int port, ws_global_context_t *const p);
