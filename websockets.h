#pragma once

#include <mutex>
#include <stdint.h>
#include <string>

#include "message.h"


typedef struct {
	std::mutex  lock;
	std::string json_data;
	uint64_t    ts;
} ws_global_context_t;

void start_websocket_thread(const int port, ws_global_context_t *const p, const bool ws_ssl_enable, const std::string & ws_ssl_cert, const std::string & ws_ssl_priv_key, const std::string & ws_ssl_ca);
void stop_websockets();

void push_to_websockets(ws_global_context_t *const ws, const std::string & json_data);
