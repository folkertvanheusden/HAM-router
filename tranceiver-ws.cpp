#include <assert.h>
#include <errno.h>
#include <optional>
#include <string>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "log.h"
#include "net.h"
#include "random.h"
#include "str.h"
#include "tranceiver-ws.h"
#include "utils.h"
#include "webserver.h"


transmit_error_t tranceiver_ws::put_message_low(const message & m)
{
	std::string json = message_to_json(m);

	push_to_websockets(&ws, json);

	return TE_ok;
}

tranceiver_ws::tranceiver_ws(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos, const int http_port, const std::string & ws_url, const int ws_port, stats *const st, const bool ws_ssl_enabled, const std::string & ws_ssl_cert, const std::string & ws_ssl_priv_key, const std::string & ws_ssl_ca) :
	tranceiver(id, s, w, pos)
{
	log(LL_INFO, "Instantiated websockets (%s)", id.c_str());

	if (ws_port != -1)
                start_websocket_thread(ws_port, &ws, ws_ssl_enabled, ws_ssl_cert, ws_ssl_priv_key, ws_ssl_ca);

	if (http_port != -1)
		webserver = start_webserver(http_port, ws_url, ws_port, st, nullptr /* TODO */);
}

tranceiver_ws::~tranceiver_ws()
{
	stop_webserver(webserver);
}

void tranceiver_ws::operator()()
{
}

tranceiver *tranceiver_ws::instantiate(const libconfig::Setting & node_in, work_queue_t *const w, const position_t & pos, stats *const st)
{
	std::string  id;
	seen        *s               = nullptr;
	int          http_port       = -1;
	int          ws_port         = -1;
	std::string  ws_url;
	bool         ws_ssl_enabled  = false;
	std::string  ws_ssl_cert;
	std::string  ws_ssl_priv_key;
	std::string  ws_ssl_ca;


        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "repetition-rate-limiting") {
			assert(s == nullptr);
			s = seen::instantiate(node);
		}
		else if (type == "http-port")
			http_port = node_in.lookup(type);
		else if (type == "websockets-port")
			ws_port = node_in.lookup(type);
		else if (type == "websockets-url")
			ws_url = node_in.lookup(type).c_str();
		else if (type == "websockets-ssl-enabled")
			ws_ssl_enabled = node_in.lookup(type);
		else if (type == "websockets-ssl-certificate")
			ws_ssl_cert = node_in.lookup(type).c_str();
		else if (type == "websockets-ssl-private-key")
			ws_ssl_priv_key = node_in.lookup(type).c_str();
		else if (type == "websockets-ssl-ca")
			ws_ssl_ca = node_in.lookup(type).c_str();
		else if (type != "type") {
			error_exit(false, "Websocket setting \"%s\" is not known", type.c_str());
		}
        }

	return new tranceiver_ws(id, s, w, pos, http_port, ws_url, ws_port, st, ws_ssl_enabled, ws_ssl_cert, ws_ssl_priv_key, ws_ssl_ca);
}
