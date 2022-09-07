#include <atomic>
#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include "error.h"
#include "log.h"
#include "str.h"
#include "time.h"
#include "utils.h"
#include "websockets.h"


#define MAX_HISTORY_SIZE 250

static std::atomic_bool  ws_terminate { false };
static std::thread      *ws_thread    { nullptr };
static lws_context      *context      { nullptr };

typedef struct {
	uint64_t ts;
} ws_session_data;

void push_to_websockets(ws_global_context_t *const ws, const std::string & json_data)
{
	ws->lock.lock();

	ws->json_data.push_back({ get_us(), json_data });

	while(ws->json_data.size() > MAX_HISTORY_SIZE)
		ws->json_data.erase(ws->json_data.begin());

	ws->lock.unlock();
}

static void send_records(lws *wsi, void *user)
{
	ws_global_context_t *wg = reinterpret_cast<ws_global_context_t *>(lws_context_user(lws_get_context(wsi)));

	ws_session_data     *ws = reinterpret_cast<ws_session_data *>(user);

	wg->lock.lock();				

	if (wg->json_data.empty() == false) {
		size_t offset = wg->json_data.size() - 1;

		while(ws->ts < wg->json_data[offset].first && offset > 0)
			offset--;

		if (ws->ts >= wg->json_data[offset].first)
			offset++;

		while(offset < wg->json_data.size()) {
			auto   & item     = wg->json_data[offset];
			size_t   data_len = item.second.size();

			uint8_t *buffer = reinterpret_cast<uint8_t *>(malloc(LWS_SEND_BUFFER_PRE_PADDING + data_len + LWS_SEND_BUFFER_POST_PADDING));

			memcpy(&buffer[LWS_SEND_BUFFER_PRE_PADDING], item.second.c_str(), data_len);

			lws_write(wsi, &buffer[LWS_SEND_BUFFER_PRE_PADDING], data_len, LWS_WRITE_TEXT);

			free(buffer);

			ws->ts = item.first;

			offset++;
		}
	}

	wg->lock.unlock();

}

static int callback_ws(struct lws *wsi, lws_callback_reasons reason, void *user, void *in, size_t len)
{
	ws_session_data     *ws = reinterpret_cast<ws_session_data *>(user);

	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED: // just log message that someone is connecting
			log(LL_DEBUG, "websocket connection established");
			lws_callback_on_writable(wsi);
			ws->ts = 0;
			send_records(wsi, user);
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE: {
			send_records(wsi, user);

			usleep(END_CHECK_INTERVAL_us);

			lws_callback_on_writable(wsi);
		}
		break;

		default:
			break;
	}

	return 0;
}

void lws_logger(int level, const char *line)
{
	log(LL_DEBUG, "libwebsockets: [%d] %s", level, line);
}

static struct lws_protocols protocols[] = {
	{
		"LoRa",        // protocol name
		callback_ws,   // callback
		sizeof(ws_session_data)
	},
	{
		nullptr, nullptr, 0
	}
};

void start_websocket_thread(const int port, ws_global_context_t *const p, const bool ws_ssl_enable, const std::string & ws_ssl_cert, const std::string & ws_ssl_priv_key, const std::string & ws_ssl_ca, db *const d)
{
	log(LL_INFO, "Starting websocket server");

	lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE, lws_logger);

	struct lws_context_creation_info context_info =
	{
		.port = port, .iface = nullptr, .protocols = protocols, .extensions = nullptr,
		.ssl_cert_filepath = ws_ssl_enable ? ws_ssl_cert.c_str() : nullptr,
		.ssl_private_key_filepath = ws_ssl_enable ? ws_ssl_priv_key.c_str() : nullptr,
		.ssl_ca_filepath = ws_ssl_enable ? ws_ssl_ca.c_str() : nullptr,
		.gid = -1, .uid = -1, .options = 0, .user = p, .ka_time = 0, .ka_probes = 0, .ka_interval = 0
	};

	context = lws_create_context(&context_info);

	if (context == nullptr)
		error_exit(false, "libwebsocket init failed");

	ws_thread = new std::thread([] {
			set_thread_name("websockets");

			for(;!ws_terminate;)
				lws_service(context, 100);
		});

	if (d) {
		time_t now = time(nullptr);
		tm *tm = localtime(&now);
		std::string today = myformat("%04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

		auto data = d->get_history("", today, true);

		for(auto & record : data)
			push_to_websockets(p, message_to_json(record));
	}
}

void stop_websockets()
{
	ws_terminate = true;

	if (ws_thread) {
		ws_thread->join();
		delete ws_thread;

		lws_context_destroy(context);
	}
}
