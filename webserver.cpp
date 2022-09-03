#include <microhttpd.h>
#include <stdio.h>
#include <string.h>

#include "db.h"
#include "log.h"
#include "stats.h"
#include "str.h"

#if MHD_VERSION < 0x00097002
// lgtm.com has a very old libmicrohttpd
#define MHD_Result int
#endif

static MHD_Daemon *d_proc { nullptr };

struct {
	stats *s;
	db    *d;
} parameters;

const std::string html_page_header = "<!DOCTYPE html>"
	"<html lang=\"en\">"
	"<head>"
	"<title>LoRa APRS gateway</title>"
	"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
	"<meta charset=\"utf-8\">"
	"</head>"
	"<body>"
	"<h1>LoRa APRS gateway</h1>\n";

std::string websocket_receiver;

const std::string html_page_footer = "\n</body>"
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

	log(LL_DEBUG_VERBOSE, "webserver: %s %s", method, url);

	std::string page;

	if (strcmp(url, "/") == 0) {
		page += html_page_header;

		page += "<p><a href=\"follow.html\">follow packets as they arrive</a></p>\n";

		auto stats_snapshot = parameters.s->snapshot();

		page += "<h2>statistics</h2>\n";

		page += "<table><tr><th>name</th><th>value</th></tr>\n";

		for(auto pair : stats_snapshot)
			page += "<tr><td>" + pair.first + "</td><td>" + pair.second + "</td></tr>\n";

		page += "</table>";
#if 0
		if (parameters.d) {
			page += "<h3>air time</h3>\n";

			auto at_records = parameters.d->get_airtime_per_callsign();

			page += "<table><tr>";
			for(auto t : at_records.first)
				page += "<th>" + t + "</th>";
			page += "</tr>\n";

			for(auto record : at_records.second) {
				page += "<tr>";

				for(auto col : record)
					page += "<td>" + col + "</td>";

				page += "</tr>\n";
			}

			page += "</table>";
		}
#endif
		page += html_page_footer;
	}
	else if (strcmp(url, "/follow.html") == 0) {
		page += html_page_header;

		page += "<table id=\"packets\" width=100%>";
		page += "<tr><th>time</th><th>source</th><th>from</th><th>to</th><th>msg id</th><th>air time</th><th>payload</th></tr>\n";
		page += "<tr><th>date</th><th>latitude</th><th>longitude</th><th>pkt crc</th><th>protocol</th><th>rssi</th></tr>\n";
		page += "</table>";

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

void * start_webserver(const int listen_port, const std::string & ws_url_in, const int ws_port, stats *const s, db *const d)
{
	if (listen_port != -1) {
		log(LL_INFO, "Starting webserver");

		parameters.s = s;
		parameters.d = d;

		std::string ws_ssl_url = myformat("'wss://' + location.hostname + ':%d/'", ws_port);
		std::string ws_url     = myformat("'ws://'  + location.hostname + ':%d/'", ws_port);

		if (ws_url_in.empty() == false) {
			ws_ssl_url = "'wss" + ws_url_in + "'";
			ws_url     = "'ws"  + ws_url_in + "'";
		}

		websocket_receiver = myformat("<script>\n"
				"function start() {\n"
				"    if (location.protocol == 'https:')\n"
				"        s = new WebSocket(%s);\n"
				"    else\n"
				"        s = new WebSocket(%s);\n"
				"    s.onclose = function() { console.log('Websocket closed'); setTimeout(function(){ start(); }, 500); };\n"
				"    s.onopen = function() { console.log('Websocket connected'); };\n"
				"    s.onmessage = function (event) {\n"
				"        try {\n"
				"            var msg = JSON.parse(event.data);\n"
				"            console.log(msg);\n"
				"            var table   = document.getElementById(\"packets\");\n"
				"            var myDate  = new Date(msg['timestamp'] * 1000);\n"
				"\n"
				"            var time    = myDate.toLocaleTimeString();\n"
				"            var date    = myDate.getFullYear() + '-' + (myDate.getMonth() + 1) + '-' + myDate.getDate();\n"
				"            var source  = msg['source'];\n"
				"            var from    = 'from' in msg ? msg['from'] : '';\n"
				"            var to      = 'to'   in msg ? msg['to'  ] : '';\n"
				"            var msg_id  = msg['msg-id'];\n"
				"            var air_t   = 'air-time' in msg ? msg['air-time' ] : '';\n"
				"            var payload = msg['payload'];\n"
				"            var lat     = 'latitude'  in msg ? msg['latitude' ].toPrecision(7) : '';\n"
				"            var lng     = 'longitude' in msg ? msg['longitude'].toPrecision(7) : '';\n"
				"            var pkt_crc = msg['pkt-crc'];\n"
				"            var prot    = msg['protocol'];\n"
				"            var rssi    = 'rssi' in msg ? msg['rssi'] : '';\n"
				"\n"
				"            var row = table.insertRow(2);\n"
				"            var cell0 = row.insertCell(0);\n"
				"	     cell0.innerHTML = date;\n"
				"            var cell1 = row.insertCell(1);\n"
				"	     cell1.innerHTML = lat;\n"
				"            var cell2 = row.insertCell(2);\n"
				"	     cell2.innerHTML = lng;\n"
				"            var cell3 = row.insertCell(3);\n"
				"            cell3.innerHTML = pkt_crc;\n"
				"            var cell4 = row.insertCell(4);\n"
				"	     cell4.innerHTML = prot;\n"
				"            var cell5 = row.insertCell(5);\n"
				"	     cell5.innerHTML = rssi;\n"
				"\n"
				"            var row = table.insertRow(2);\n"
				"            var cell0 = row.insertCell(0);\n"
				"	     cell0.innerHTML = time;\n"
				"            var cell1 = row.insertCell(1);\n"
				"	     cell1.innerHTML = source;\n"
				"            var cell2 = row.insertCell(2);\n"
				"	     cell2.innerHTML = from;\n"
				"            var cell3 = row.insertCell(3);\n"
				"	     cell3.innerHTML = to;\n"
				"            var cell4 = row.insertCell(4);\n"
				"	     cell4.innerHTML = msg_id;\n"
				"            var cell5 = row.insertCell(5);\n"
				"	     cell5.innerHTML = air_t;\n"
				"            var cell6 = row.insertCell(6);\n"
				"	     cell6.innerHTML = payload;\n"
				"\n"
				"            var rowCount = table.rows.length;\n"
				"            if (rowCount > 250) {\n"
				"		    table.deleteRow(rowCount -1);\n"
				"		    table.deleteRow(rowCount -1);\n"
				"            }\n"
				"        }\n"
				"        catch (error) {\n"
				"            console.error(error);\n"
				"        }\n"
				"    };\n"
				"};\n"
				"document.addEventListener('DOMContentLoaded', function() { start(); });\n"
				"</script>\n", ws_ssl_url.c_str(), ws_url.c_str());

		d_proc = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
			       listen_port,
			       nullptr,
			       nullptr,
			       process_http_request,
			       nullptr,
			       MHD_OPTION_END);

		return d_proc;
	}

	return nullptr;
}

void stop_webserver(void *d)
{
	if (d)
		MHD_stop_daemon(reinterpret_cast<MHD_Daemon *>(d_proc));
}
