#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include "error.h"
#include "log.h"
#include "websockets.h"

typedef struct {
	uint64_t ts;
} ws_session_data;

static int callback_http(struct lws *wsi, lws_callback_reasons reason, void *user, void *in, size_t len)
{
	return 0;
}

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

			lws_callback_on_writable(wsi);
		}

		default:
			break;
	}

	return 0;
}


static struct lws_protocols protocols[] = {
	/* first protocol must always be HTTP handler */
	{
		"http-only",   // name
		callback_http, // callback
		0              // per_session_data_size
	},
	{
		"LoRa",        // protocol name
		callback_ws,   // callback
		sizeof(ws_session_data)
	},
	{
		nullptr, nullptr, 0
	}
};

void start_websocket_thread(const int port, ws_global_context_t *const p)
{
	log(LL_INFO, "Starting websocket server");

	struct lws_context_creation_info context_info =
	{
		.port = port, .iface = NULL, .protocols = protocols, .extensions = NULL,
		.ssl_cert_filepath = NULL, .ssl_private_key_filepath = NULL, .ssl_ca_filepath = NULL,
		.gid = -1, .uid = -1, .options = 0, .user = p, .ka_time = 0, .ka_probes = 0, .ka_interval = 0
	};

	struct lws_context *context = lws_create_context(&context_info);

	if (context == nullptr)
		error_exit(false, "libwebsocket init failed");

	std::thread websocket_thread([context] {
			for(;;)
				lws_service(context, 1000);
		});

	websocket_thread.detach();
}
