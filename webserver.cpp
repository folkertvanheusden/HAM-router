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

std::string get_html_page_header(const bool wide) 
{
	return std::string("<!DOCTYPE html>"
		"<html lang=\"en\">"
		"<head>"
		"<title>LoRa APRS gateway</title>"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
		"<link href=\"") + (wide ? "stylesheet-w.css" : "stylesheet.css") + "\" rel=\"stylesheet\" type=\"text/css\">"
		"<meta charset=\"utf-8\">"
		"</head>"
		"<body>"
		"<h1>HAM router</h1>\n";
}

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

	std::string work_url = url;

	std::string page;

	if (work_url == "/") {
		page += get_html_page_header(false);

		page += "<p><a href=\"follow.html\">follow packets as they arrive</a></p>\n";

		page += "<p><a href=\"counters.html\">counters</a></p>\n";

		if (parameters.d) {
			parameters.d->get_heard_counts();

			page += "<h3>global counts</h3>\n";

			auto global_counts = parameters.d->get_misc_counts();

			page += "<table><tr><th>what</th><th>count</th></tr>\n";

			for(auto row : global_counts)
				page += "<tr><td>" + row.first + "</td><td>" + std::to_string(row.second) + "</td></tr>\n";

			page += "</table>";

			page += "<h3>\"mheard\" (from)</h3>\n";
			page += "<p>Click on a callsign to jump to APRS-SI</p>\n";

			auto hc_rows = parameters.d->get_heard_counts();

			page += "<table><tr><th>callsign</th><th>count</th></tr>\n";

			for(auto row : hc_rows)
				page += "<tr><td><a href=\"https://aprs.fi/#!mt=roadmap&z=11&call=a%2F" + row.first + "&timerange=3600&tail=3600\">" + row.first + "</a></td><td>" + std::to_string(row.second) + "</td></tr>\n";

			page += "</table>";

			page += "<h3>air time</h3>\n";

			auto at_rows = parameters.d->get_air_time();

			page += "<table><tr><th>callsign</th><th>date</th><th>sum</th></tr>\n";

			page += "<p>Click on a date to jump to show packets of that date for that callsign</p>\n";

			for(auto row : at_rows) {
				std::string current_callsign = row.first.first;

				std::string time             = row.second > 0 ? myformat("%.2f", row.second) + "s" : "-";

				if (current_callsign.empty())
					current_callsign = "[unknown]";

				page += "<tr><td>" + current_callsign + "</a></td><td><a href=\"history.html?callsign=" + row.first.first + "&date=" + row.first.second + "\">" + row.first.second + "</a></td><td>" + time + "</td></tr>\n";
			}

			page += "</table>";

			page += "<h3>destiny</h3>\n";

			auto d_rows = parameters.d->get_to_counts();

			page += "<table><tr><th>callsign</th><th>count</th></tr>\n";

			for(auto row : d_rows)
				page += "<tr><td>" + row.first + "</td><td>" + std::to_string(row.second) + "</td></tr>\n";

			page += "</table>";

			page += "<h3>protocol counts</h3>\n";

			auto pc_rows = parameters.d->get_protocol_counts();

			page += "<table><tr><th>callsign</th><th>count</th></tr>\n";

			for(auto row : pc_rows)
				page += "<tr><td>" + row.first + "</td><td>" + std::to_string(row.second) + "</td></tr>\n";

			page += "</table>";
		}

		page += html_page_footer;
	}
	else if (work_url == "/counters.html") {
		page += get_html_page_header(false);

		page += "<h2>counters</h2>\n";

		page += "<table><tr><th>name</th><th>value</th></tr>\n";

		auto stats_snapshot = parameters.s->snapshot();

		for(auto pair : stats_snapshot)
			page += "<tr><td>" + pair.first + "</td><td>" + pair.second + "</td></tr>\n";

		page += "</table>";

		page += html_page_footer;
	}
	else if (work_url == "/follow.html") {
		page += get_html_page_header(true);

		page += "<table id=\"packets\" width=100%>";
		page += "<tr><th>time</th><th>source</th><th>from</th><th>to</th><th>msg id</th><th>air time</th><th>payload</th></tr>\n";
		page += "<tr><th>date</th><th>latitude</th><th>longitude</th><th>pkt crc</th><th>protocol</th><th>rssi</th></tr>\n";
		page += "</table>";

		page += websocket_receiver;

		page += html_page_footer;
	}
	else if (work_url == "/stylesheet.css" || work_url == "/stylesheet-w.css") {
		bool wide = work_url == "/stylesheet-w.css";

		page = myformat(":root{--sans-font:-apple-system,BlinkMacSystemFont,\"Avenir Next\",Avenir,\"Nimbus Sans L\",Roboto,Noto,\"Segoe UI\",Arial,Helvetica,\"Helvetica Neue\",sans-serif;--mono-font:Consolas,Menlo,Monaco,\"Andale Mono\",\"Ubuntu Mono\",monospace;--base-fontsize:1.15rem;--header-scale:1.25;--line-height:1.618;--bg:#FFF;--accent-bg:#F5F7FF;--text:#212121;--text-light:#585858;--border:#D8DAE1;--accent:#0D47A1;--accent-light:#90CAF9;--code:#D81B60;--preformatted:#444;--marked:#FFDD33;--disabled:#EFEFEF}@media (prefers-color-scheme:dark){:root{--bg:#212121;--accent-bg:#2B2B2B;--text:#DCDCDC;--text-light:#ABABAB;--border:#666;--accent:#FFB300;--accent-light:#FFECB3;--code:#F06292;--preformatted:#CCC;--disabled:#111}img,video{opacity:.6}}html{font-family:var(--sans-font)}body{color:var(--text);background:var(--bg);font-size:var(--base-fontsize);line-height:var(--line-height);min-height:100vh;margin:0 auto;max-width:%drem;padding:0 .5rem;overflow-x:hidden;word-break:break-word;overflow-wrap:break-word}header{background:var(--accent-bg);border-bottom:1px solid var(--border);text-align:center;padding:2rem .5rem;width:100vw;position:relative;box-sizing:border-box;left:50%;right:50%;margin-left:-50vw;margin-right:-50vw}header h1,header p{margin:0}h1,h2,h3{line-height:1.1}nav{font-size:1rem;line-height:2;padding:1rem 0}nav a{margin:1rem 1rem 0 0;border:1px solid var(--border);border-radius:5px;color:var(--text)!important;display:inline-block;padding:.1rem 1rem;text-decoration:none;transition:.4s}nav a:hover{color:var(--accent)!important;border-color:var(--accent)}nav a.current:hover{text-decoration:none}footer{margin-top:4rem;padding:2rem 1rem 1.5rem 1rem;color:var(--text-light);font-size:.9rem;text-align:center;border-top:1px solid var(--border)}h1{font-size:calc(var(--base-fontsize) * var(--header-scale) * var(--header-scale) * var(--header-scale) * var(--header-scale));margin-top:calc(var(--line-height) * 1.5rem)}h2{font-size:calc(var(--base-fontsize) * var(--header-scale) * var(--header-scale) * var(--header-scale));margin-top:calc(var(--line-height) * 1.5rem)}h3{font-size:calc(var(--base-fontsize) * var(--header-scale) * var(--header-scale));margin-top:calc(var(--line-height) * 1.5rem)}h4{font-size:calc(var(--base-fontsize) * var(--header-scale));margin-top:calc(var(--line-height) * 1.5rem)}h5{font-size:var(--base-fontsize);margin-top:calc(var(--line-height) * 1.5rem)}h6{font-size:calc(var(--base-fontsize)/ var(--header-scale));margin-top:calc(var(--line-height) * 1.5rem)}a,a:visited{color:var(--accent)}a:hover{text-decoration:none}[role=button],a button,button,input[type=button],input[type=reset],input[type=submit]{border:none;border-radius:5px;background:var(--accent);font-size:1rem;color:var(--bg);padding:.7rem .9rem;margin:.5rem 0;transition:.4s}[role=button][aria-disabled=true],a button[disabled],button[disabled],input[type=button][disabled],input[type=checkbox][disabled],input[type=radio][disabled],input[type=reset][disabled],input[type=submit][disabled],select[disabled]{cursor:default;opacity:.5;cursor:not-allowed}input:disabled,select:disabled,textarea:disabled{cursor:not-allowed;background-color:var(--disabled)}input[type=range]{padding:0}abbr{cursor:help}[role=button]:focus,[role=button]:not([aria-disabled=true]):hover,button:enabled:hover,button:focus,input[type=button]:enabled:hover,input[type=button]:focus,input[type=checkbox]:enabled:hover,input[type=checkbox]:focus,input[type=radio]:enabled:hover,input[type=radio]:focus,input[type=reset]:enabled:hover,input[type=reset]:focus,input[type=submit]:enabled:hover,input[type=submit]:focus{opacity:.8;cursor:pointer}details{background:var(--accent-bg);border:1px solid var(--border);border-radius:5px;margin-bottom:1rem}summary{cursor:pointer;font-weight:700;padding:.6rem 1rem}details[open]{padding:.6rem 1rem .75rem 1rem}details[open] summary{margin-bottom:.5rem;padding:0}details[open]>:last-child{margin-bottom:0}table{border-collapse:collapse;width:100%;margin:1.5rem 0}td,th{border:1px solid var(--border);text-align:left;padding:.5rem}th{background:var(--accent-bg);font-weight:700}tr:nth-child(even){background:var(--accent-bg)}table caption{font-weight:700;margin-bottom:.5rem}ol,ul{padding-left:3rem}input,select,textarea{font-size:inherit;font-family:inherit;padding:.5rem;margin-bottom:.5rem;color:var(--text);background:var(--bg);border:1px solid var(--border);border-radius:5px;box-shadow:none;box-sizing:border-box;width:60%;-moz-appearance:none;-webkit-appearance:none;appearance:none}select{background-image:linear-gradient(45deg,transparent 49%,var(--text) 51%),linear-gradient(135deg,var(--text) 51%,transparent 49%);background-position:calc(100% - 20px),calc(100% - 15px);background-size:5px 5px,5px 5px;background-repeat:no-repeat}select[multiple]{background-image:none!important}input[type=checkbox],input[type=radio]{vertical-align:bottom;position:relative}input[type=radio]{border-radius:100%}input[type=checkbox]:checked,input[type=radio]:checked{background:var(--accent)}input[type=checkbox]:checked::after{content:' ';width:.1em;height:.25em;border-radius:0;position:absolute;top:.05em;left:.18em;background:0 0;border-right:solid var(--bg) .08em;border-bottom:solid var(--bg) .08em;font-size:1.8em;transform:rotate(45deg)}input[type=radio]:checked::after{content:' ';width:.25em;height:.25em;border-radius:100%;position:absolute;top:.125em;background:var(--bg);left:.125em;font-size:32px}textarea{width:80%}@media only screen and (max-width:720px){input,select,textarea{width:100%}}input[type=checkbox],input[type=radio]{width:auto}input[type=file]{border:0}fieldset{border:0;padding:0;margin:0}hr{color:var(--border);border-top:1px;margin:1rem auto}mark{padding:2px 5px;border-radius:4px;background:var(--marked)}main img,main video{max-width:100%;height:auto;border-radius:5px}figure{margin:0}figcaption{font-size:.9rem;color:var(--text-light);text-align:center;margin-bottom:1rem}blockquote{margin:2rem 0 2rem 2rem;padding:.4rem .8rem;border-left:.35rem solid var(--accent);opacity:.8;font-style:italic}cite{font-size:.9rem;color:var(--text-light);font-style:normal}code,kbd,pre,pre span,samp{font-size:1.075rem;font-family:var(--mono-font);color:var(--code)}kbd{color:var(--preformatted);border:1px solid var(--preformatted);border-bottom:3px solid var(--preformatted);border-radius:5px;padding:.1rem}pre{padding:1rem 1.4rem;max-width:100%;overflow:auto;overflow-x:auto;color:var(--preformatted);background:var(--accent-bg);border:1px solid var(--border);border-radius:5px}pre code{color:var(--preformatted);background:0 0;margin:0;padding:0}", wide ? 125 : 70);
	}
	else if (work_url == "/history.html") {
		page += get_html_page_header(true);

		if (parameters.d == nullptr)
			page = "Not available: no traffic database configured";
		else {
			const char  *p_callsign = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "callsign");
			std::string  callsign = p_callsign ? p_callsign : "";

			const char  *p_date = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "date");
			if (!p_date)
				return MHD_NO;

			std::string  date   = p_date;

			auto history = parameters.d->get_history(callsign, date, false);

			page += "<h2>History for " + callsign + "</h2>";

			page += "<table id=\"packets\" width=100%>";
			page += "<tr><th>date/time</th><th>source</th><th>from</th><th>to</th><th>msg id</th><th>payload</th></tr>\n";
			page += "<tr><th></th><th>latitude</th><th>longitude</th><th>air time</th><th>rssi</th><th>protocol</th><th</tr>\n";

			for(auto & record : history) {
				auto        meta  = record.get_meta();

				auto        from_it  = meta.find("from"    );
				std::string from  = from_it != meta.end() ? from_it ->second.s_value : "";

				auto        to_it    = meta.find("to"      );
				std::string to    = to_it   != meta.end() ? to_it   ->second.s_value : "";

				auto        proto_it = meta.find("protocol");
				std::string proto = proto_it!= meta.end() ? proto_it->second.s_value : "";

				time_t      t     = record.get_tv().tv_sec;
				tm        * tm    = localtime(&t);

				char ts_buffer_date[64] { 0 };
				char ts_buffer_time[64] { 0 };

				strftime(ts_buffer_date, sizeof(ts_buffer_date), "%Y-%m-%d", tm);
				strftime(ts_buffer_time, sizeof(ts_buffer_time), "%H:%M:%S", tm);

				auto        payload_it = record.get_meta().find("payload");

				std::string payload    = payload_it != record.get_meta().end() ? payload_it->second.s_value : "";

				std::string latitude   = myformat("%.8f", meta.find("latitude")  != meta.end() ? meta.at("latitude" ).d_value : 0.);

				std::string longitude  = myformat("%.8f", meta.find("longitude") != meta.end() ? meta.at("longitude").d_value : 0.);

				std::string rssi       = meta.find("rssi") != meta.end() ? meta.at("rssi").s_value : "";

				std::string air_time   = myformat("%.2f", meta.find("air-time")  != meta.end() ? meta.at("air-time" ).d_value : 0.);

				page += "<tr><td>" + std::string(ts_buffer_date) + "</td><td>" + record.get_source() + "</td><td>" + from + "</td><td>" + to + "</td><td>" + record.get_id_short() + "</td><td>" + payload + "</td></tr>";

				page += "<tr><td>" + std::string(ts_buffer_time) + "</td><td>" + latitude + "</td><td> " + longitude + "</td><td>" + air_time + "</td><td> " + rssi + "</td><td>" + proto + "</td></tr>\n";
			}

			page += "</table>";
		}

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

		websocket_receiver = myformat("<script defer>\n"
				"col = 0;\n"
				"function start() {\n"
				"    if (location.protocol == 'https:')\n"
				"        s = new WebSocket(%s);\n"
				"    else\n"
				"        s = new WebSocket(%s);\n"
				"    s.onclose = function() { console.log('Websocket closed'); setTimeout(function(){ start(); }, 500); };\n"
				"    s.onopen = function() { console.log('Websocket connected'); };\n"
				"    s.onmessage = function (event) {\n"
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
				"            if (col == 0) { row.style.background = \"#9090ff\"; } else { row.style.background = \"#70ff70\"; }\n"
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
				"            var cell6 = row.insertCell(6);\n"
				"	     cell6.innerHTML = '';\n"
				"\n"
				"            var row = table.insertRow(2);\n"
				"            if (col == 0) { row.style.background = \"#d0d0ff\"; } else { row.style.background = \"#b0ffb0\"; }\n"
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
				"            for(;;) {\n"
				"                var rowCount = table.rows.length;\n"
				"                if (rowCount < 250) break;\n"

				"		 table.deleteRow(rowCount -1);\n"
				"		 table.deleteRow(rowCount -2);\n"
				"            }\n"
				"\n"
				"	     col = 1 - col;\n"
				"    };\n"
				"};\n"
				"start();\n"
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
