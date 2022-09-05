#include <errno.h>
#include <optional>
#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "ax25.h"
#include "crc_ppp.h"
#include "error.h"
#include "gps.h"
#include "log.h"
#include "net.h"
#include "random.h"
#include "str.h"
#include "time.h"
#include "tranceiver-beacon.h"
#include "utils.h"


tranceiver_beacon::tranceiver_beacon(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos, const std::string & beacon_text, const int beacon_interval, const beacon_mode_t bm, const std::string & callsign) :
	tranceiver(id, s, w, pos),
	beacon_text(beacon_text),
	beacon_interval(beacon_interval),
	bm(bm),
	callsign(callsign)
{
	log(LL_INFO, "Instantiated beacon (%s)", id.c_str());

	th = new std::thread(std::ref(*this));
}

tranceiver_beacon::~tranceiver_beacon()
{
}

transmit_error_t tranceiver_beacon::put_message_low(const message & m)
{
	return TE_hardware;
}

void tranceiver_beacon::operator()()
{
	set_thread_name("t-beacon");

	log(LL_INFO, "Beacon: started thread");

	sleep(1);

        for(;!terminate;) {
		message *m { nullptr };

		timeval tv { 0 };
		gettimeofday(&tv, nullptr);

		std::string source = myformat("beacon(%s)", get_id().c_str());
		uint64_t    msg_id = get_random_uint64_t();

		if (bm == beacon_mode_aprs) {
			std::string aprs_text = "!" + gps_double_to_aprs(local_pos.latitude, local_pos.longitude) + "[";

			std::string output = "<\xff\x01" + callsign + "-L>APLG01,TCPIP*,qAC:" + aprs_text + beacon_text;

			m = new message(tv,
					source,
					msg_id,
					reinterpret_cast<const uint8_t *>(output.c_str()),
					output.size());
		}
		else if (bm == beacon_mode_ax25) {
			ax25 packet;

			std::string temp = callsign;
			char        ssid = ' ';

			std::size_t pos_min = callsign.find('-');
			if (pos_min != std::string::npos) {
				temp = callsign.substr(0, pos_min);

				ssid = callsign.substr(pos_min + 1)[0];
			}

			packet.set_from(temp, ssid, true, false);
			packet.set_to  ("IDENT", ' ', false, false);
			packet.set_control(3  );  // U frame (unnumbered)
			packet.set_pid    (240);  // no layer 3
			packet.set_data(reinterpret_cast<const uint8_t *>(beacon_text.c_str()), beacon_text.size());

			auto packet_binary = packet.generate_packet();

			m = new message(tv,
					source,
					msg_id,
					packet_binary.first,
					packet_binary.second);

			free(packet_binary.first);
		}
		else {
			log(LL_INFO, "UNEXPECTED BEACON MODE");

			break;
		}

		log(LL_INFO, "Send beacon %s", m->get_id_short().c_str());

		queue_incoming_message(*m);

		delete m;

		if (!myusleep(beacon_interval * 1000000ll, &terminate))
			break;
        }
}

tranceiver *tranceiver_beacon::instantiate(const libconfig::Setting & node_in, work_queue_t *const w, const position_t & pos)
{
	std::string   id;
	seen         *s               = nullptr;
	std::string   beacon_text     = "Hello, this is dog!";
	int           beacon_interval = 60;
	beacon_mode_t bm              = beacon_mode_ax25;
	std::string   callsign;

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "text")
			beacon_text = node_in.lookup(type).c_str();
		else if (type == "source-callsign")
			callsign = node_in.lookup(type).c_str();
		else if (type == "interval")
			beacon_interval = node_in.lookup(type);
		else if (type == "mode") {
			std::string mode = node_in.lookup(type).c_str();

			if (mode == "aprs")
				bm = beacon_mode_aprs;
			else if (mode == "ax25")
				bm = beacon_mode_ax25;
			else
				error_exit(false, "beacon(line %d): beacon mode \"%s\" is not known", node.getSourceLine(), mode.c_str());
		}
		else if (type != "type") {
			error_exit(false, "beacon(line %d): setting \"%s\" is not known", node.getSourceLine(), type.c_str());
		}
        }

	if (callsign.empty())
		error_exit(false, "beacon(line %d): beacons need a source-callsign configured", node_in.getSourceLine());

	return new tranceiver_beacon(id, s, w, pos, beacon_text, beacon_interval, bm, callsign);
}
