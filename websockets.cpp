#include <atomic>
#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include "error.h"
#include "log.h"
#include "utils.h"
#include "websockets.h"

static std::atomic_bool  ws_terminate { false };
static std::thread      *ws_thread    { nullptr };
static lws_context      *context      { nullptr };

typedef struct {
	uint64_t ts;
} ws_session_data;

static int callback_ws(struct lws *wsi, lws_callback_reasons reason, void *user, void *in, size_t len)
{
	ws_session_data     *ws = reinterpret_cast<ws_session_data *>(user);

	ws_global_context_t *wg = reinterpret_cast<ws_global_context_t *>(lws_context_user(lws_get_context(wsi)));

	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED: // just log message that someone is connecting
			log(LL_DEBUG, "websocket connection established");
			lws_callback_on_writable(wsi);
			ws->ts = 0;
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE: {
			std::string data;

			wg->lock.lock();				

			if (wg->ts > ws->ts) {
				ws->ts = wg->ts;
				data = wg->json_data;
			}

			wg->lock.unlock();

			if (data.empty() == false) {
				size_t data_len = data.size();

				uint8_t *buf = reinterpret_cast<uint8_t *>(malloc(LWS_SEND_BUFFER_PRE_PADDING + data_len + LWS_SEND_BUFFER_POST_PADDING));

				memcpy(&buf[LWS_SEND_BUFFER_PRE_PADDING], data.c_str(), data_len);

				lws_write(wsi, &buf[LWS_SEND_BUFFER_PRE_PADDING], data_len, LWS_WRITE_TEXT);

				free(buf);
			}

			usleep(251000);

			lws_callback_on_writable(wsi);
		}
		break;

		default:
			log(LL_DEBUG_VERBOSE, "callback_ws, reason: %d (length: %d)", reason, len);
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

void start_websocket_thread(const int port, ws_global_context_t *const p, const bool ws_ssl_enable, const std::string & ws_ssl_cert, const std::string & ws_ssl_priv_key, const std::string & ws_ssl_ca)
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
			for(;!ws_terminate;) {
				try {
					lws_service(context, 100);
		                }
				catch(const std::exception& e) {
					log(LL_ERR, "ws_thread: exception %s", e.what());
				}
			}
		});
}

void stop_websockets()
{
	ws_terminate = true;

	ws_thread->join();
	delete ws_thread;

	lws_context_destroy(context);
}

void push_to_websockets(ws_global_context_t *const ws, const std::string & json_data)
{
	ws->lock.lock();
	ws->json_data = json_data;
	ws->ts        = get_us();
	ws->lock.unlock();
}
