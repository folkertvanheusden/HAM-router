#pragma once

#include <mutex>
#include <stdint.h>
#include <string>
#include <vector>

#include "db.h"
#include "message.h"


typedef struct {
	std::mutex  lock;

	std::vector<std::pair<uint64_t, std::string> > json_data;
} ws_global_context_t;

void start_websocket_thread(const int port, ws_global_context_t *const p, const bool ws_ssl_enable, const std::string & ws_ssl_cert, const std::string & ws_ssl_priv_key, const std::string & ws_ssl_ca, db *const d);
void stop_websockets();

void push_to_websockets(ws_global_context_t *const ws, const std::string & json_data);
