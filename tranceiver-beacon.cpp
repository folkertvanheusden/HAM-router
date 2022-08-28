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

#include "crc_ppp.h"
#include "error.h"
#include "log.h"
#include "net.h"
#include "tranceiver-beacon.h"
#include "utils.h"


tranceiver_beacon::tranceiver_beacon(const std::string & id, seen *const s, work_queue_t *const w, const std::string & beacon_text, const int beacon_interval, const beacon_mode_t bm, const std::string & callsign) :
	tranceiver(id, s, w),
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

transmit_error_t tranceiver_beacon::put_message_low(const uint8_t *const p, const size_t s)
{
	return TE_hardware;
}

void tranceiver_beacon::operator()()
{
	log(LL_INFO, "Beacon: started thread");

	sleep(1);

        for(;;) {
		log(LL_INFO, "Send beacon");

		message_t m { 0 };
		gettimeofday(&m.tv, nullptr);

		if (bm == beacon_mode_aprs) {
			std::string output = ">\xff\x01" + callsign + "-L>APLG01,TCPIP*,qAC:" + beacon_text;

			m.message = reinterpret_cast<uint8_t *>(strdup(output.c_str()));
			m.s       = output.size();
		}	
		else {
			log(LL_INFO, "UNEXPECTED BEACON MODE");
		}

		queue_incoming_message(m);

		sleep(beacon_interval);
        }
}

tranceiver *tranceiver_beacon::instantiate(const libconfig::Setting & node_in, work_queue_t *const w)
{
	std::string   id;
	seen         *s               = nullptr;
	std::string   beacon_text     = "Hello, this is dog!";
	int           beacon_interval = 60;
	beacon_mode_t bm              = beacon_mode_ax25;
	std::string   callsign        = "mycallsign";

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "text")
			beacon_text = node_in.lookup(type).c_str();
		else if (type == "callsign")
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
				error_exit(false, "beacon mode \"%s\" is now known", mode.c_str());
		}
		else if (type != "type") {
			error_exit(false, "setting \"%s\" is now known", type.c_str());
		}
        }

	return new tranceiver_beacon(id, s, w, beacon_text, beacon_interval, bm, callsign);
}
