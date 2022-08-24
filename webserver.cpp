#include <microhttpd.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "stats.h"
#include "utils.h"

static int ws_port = -1;

const std::string html_page_header = "<!DOCTYPE html>"
	"<html lang=\"en\">"
	"<head>"
	"<title>LoRa APRS gateway</title>"
	"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
	"<meta charset=\"utf-8\">"
	"</head>"
	"<body>"
	"<h1>LoRa APRS gateway</h1>";

std::string websocket_receiver;

const std::string html_page_footer = "</body>"
	"</html>";

MHD_Result process_http_request(void *cls,
         struct MHD_Connection *connection,
         const char *url,
         const char *method,
         const char *version,
         const char *upload_data,
         size_t *upload_data_size,
         void **ptr)
{
	// MHD_RESPMEM_MUST_COPY

	if (strcmp(method, "GET") != 0)
		return MHD_NO; 

	std::string page;

	if (strcmp(url, "/") == 0) {
		page += html_page_header;

		page += "<table><tr><th>packets</th></tr>";
		page += "<tr><td id=\"packets\"></td></tr></table>";

		page += "<a href=\"/follow.html\">follow packets as they arrive</a>\n";

		page += html_page_footer;
	}
	else if (strcmp(url, "/follow.html") == 0) {
		page += html_page_header;

		page += websocket_receiver;

		page += html_page_footer;
	}

	if (page.empty())
		return MHD_NO;

	MHD_Response *response = MHD_create_response_from_buffer(page.size(), const_cast<char *>(page.c_str()), MHD_RESPMEM_MUST_COPY);
	MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);

	return ret;
}

void start_webserver(const int listen_port, const int ws_port_in, stats *const s)
{
	if (listen_port != -1) {
		log(LL_INFO, "Starting webserver");

		ws_port = ws_port_in;

		websocket_receiver = myformat("<script>"
				"function start() {"
				"if (location.protocol == 'https:')"
				"    s = new WebSocket('wss://' + location.hostname + ':%d/');"
				"else"
				"    s = new WebSocket('ws://' + location.hostname + ':%d/');"
				"s.onclose = function() { console.log('Websocket closed'); setTimeout(function(){ start(); }, 500); };"
				"s.onopen = function() { console.log('Websocket connected'); };"
				"s.onmessage = function (event) {"
				"    try {"
				"        var msg = JSON.parse(event.data);"
				"        console.log(msg);"
				"        var target = document.getElementById(\"packets\");"
				"        target.textContent = msg['callsign-from'] + \" =&gt; \" + msg['callsign-to'] + \" (\" + msg['distance'] + \"): \" + msg['data'] + target.textContent;"
				"    }"
				"    catch (error) "
				"        console.error(error);"
				"    }"
				"}"
				"start();"
				"</script>", ws_port, ws_port);

		/* MHD_Daemon *d = */MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
			       listen_port,
			       nullptr,
			       nullptr,
			       process_http_request,
			       s,
			       MHD_OPTION_END);
	}
}
