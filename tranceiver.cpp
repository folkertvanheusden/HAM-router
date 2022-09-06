#include "dissect-packet.h"
#include "error.h"
#include "log.h"
#include "log.h"
#include "str.h"
#include "time.h"
#include "utils.h"
#include "tranceiver.h"
#include "tranceiver-aprs-si.h"
#include "tranceiver-axudp.h"
#include "tranceiver-beacon.h"
#include "tranceiver-db.h"
#include "tranceiver-kiss-kernel.h"
#include "tranceiver-kiss-tty.h"
#include "tranceiver-lora-sx1278.h"
#include "tranceiver-mqtt.h"
#include "tranceiver-ws.h"


tranceiver::tranceiver(const std::string & id, seen *const s, work_queue_t *const w, const position_t & local_pos) :
	id(id),
	w(w),
	s(s),
	local_pos(local_pos)
{
}

tranceiver::~tranceiver()
{
	delete s;
}

void tranceiver::stop()
{
	terminate = true;

	if (th) {
		th->join();

		delete th;
	}

	if (s)
		s->stop();
}

transmit_error_t tranceiver::queue_incoming_message(const message & m)
{
	auto content = m.get_content();

	stats_add_counter(ifInOctets,   content.second);
	stats_add_counter(ifHCInOctets, content.second);
	stats_inc_counter(ifInUcastPkts);

	// check if s was allocated because e.g. the beacon module does
	// not allocate a seen object
	uint32_t hash       = 0;
	bool     hash_valid = false;

	if (s) {
		auto ratelimit_rc = s->check(content.first, content.second);

		if (ratelimit_rc.first == false) {
			log(LL_DEBUG, "tranceiver::queue_incoming_message(%s: %s): dropped because of duplicates rate limiting", id.c_str(), m.get_id_short().c_str());

			return TE_ratelimiting;
		}

		hash       = ratelimit_rc.second;
		hash_valid = true;
	}

	// push to incoming queue of this tranceiver (TODO: empty when not consuming (eg beacon))
	{
		message copy { m };

		auto    meta = dissect_packet(content.first, content.second);

		if (meta.has_value()) {
			if (meta.value().find("latitude") != meta.value().end() && meta.value().find("longitude") != meta.value().end()) {
				double cur_lat = meta.value().find("latitude")->second.d_value;
				double cur_lng = meta.value().find("longitude")->second.d_value;

				double distance = calcGPSDistance(cur_lat, cur_lng, local_pos.latitude, local_pos.longitude);

				meta.value().insert({ "distance", myformat("%.2f", distance) });
			}

			if (hash_valid)
				meta.value().insert({ "pkt-crc", myformat("%08x", hash) });

			copy.set_meta(meta.value());
		}

		std::unique_lock<std::mutex> lck(incoming_lock);

		incoming.push(copy);

		incoming_cv.notify_all();

		log(LL_DEBUG, "tranceiver::queue_incoming_message(%s: %s): message queued", id.c_str(), m.get_id_short().c_str());
	}

	// let main know that there's work to process
	{
		std::unique_lock<std::mutex> lck(w->work_lock);

		w->work_list.push(this);

		w->work_cv.notify_one();
	}

	return TE_ok;
}

bool tranceiver::peek()
{
	std::unique_lock<std::mutex> lck(incoming_lock);

	return incoming.empty() == false;
}

std::optional<message> tranceiver::get_message()
{
	std::unique_lock<std::mutex> lck(incoming_lock);

	while(incoming.empty()) {
		if (incoming_cv.wait_for(lck, std::chrono::milliseconds(END_CHECK_INTERVAL_ms)) == std::cv_status::timeout)
			return { };
	}

	auto rc = incoming.front();
	incoming.pop();

	return rc;
}

transmit_error_t tranceiver::put_message(const message & m)
{
	size_t size = m.get_content().second;

	stats_add_counter(ifOutOctets,   size);
	stats_add_counter(ifHCOutOctets, size);
	stats_inc_counter(ifOutUcastPkts);

	return put_message_low(m);
}

tranceiver *tranceiver::instantiate(const libconfig::Setting & node, work_queue_t *const w, const position_t & pos, stats *const st, int device_nr, ws_global_context_t *const ws, const std::vector<tranceiver *> & tranceivers)
{
	std::string type = node.lookup("type").c_str();

	tranceiver *t = nullptr;

	if (type == "aprs-si") {
		t = tranceiver_aprs_si::instantiate(node, w, pos, st, device_nr);
	}
	else if (type == "kiss-kernel") {
		t = tranceiver_kiss_kernel::instantiate(node, w, pos);
	}
	else if (type == "kiss-tty") {
		t = tranceiver_kiss_tty::instantiate(node, w, pos);
	}
	else if (type == "lora-sx1278") {
		t = tranceiver_lora_sx1278::instantiate(node, w, pos, st, device_nr);
	}
	else if (type == "axudp") {
		t = tranceiver_axudp::instantiate(node, w, pos);
	}
	else if (type == "beacon") {
		t = tranceiver_beacon::instantiate(node, w, pos);
	}
	else if (type == "database") {
		t = tranceiver_db::instantiate(node, w, pos);
	}
	else if (type == "mqtt") {
		t = tranceiver_mqtt::instantiate(node, w, pos);
	}
	else if (type == "http-ws") {
		t = tranceiver_ws::instantiate(node, w, pos, st, tranceivers);
	}
	else {
		error_exit(false, "(line %d): \"%s\" is an unknown tranceiver type", node.getSourceLine(), type.c_str());
	}

	return t;
}

void tranceiver::register_snmp_counters(stats *const st, const int device_nr)
{
        ifInOctets     = st->register_stat(myformat("%s-ifInOctets",     get_id().c_str()), myformat("1.3.6.1.2.1.2.2.1.10.%zu",    device_nr), snmp_integer::si_counter32);
        ifHCInOctets   = st->register_stat(myformat("%s-ifHCInOctets",   get_id().c_str()), myformat("1.3.6.1.2.1.31.1.1.1.6.%zu",  device_nr), snmp_integer::si_counter64);
        ifInUcastPkts  = st->register_stat(myformat("%s-ifInUcastPkts",  get_id().c_str()), myformat("1.3.6.1.2.1.2.2.1.11.%zu",    device_nr), snmp_integer::si_counter32);

        ifOutOctets    = st->register_stat(myformat("%s-ifOutOctets",    get_id().c_str()), myformat("1.3.6.1.2.1.2.2.1.16.%zu",    device_nr), snmp_integer::si_counter32);
        ifHCOutOctets  = st->register_stat(myformat("%s-ifHCOutOctets",  get_id().c_str()), myformat("1.3.6.1.2.1.31.1.1.1.10.%zu", device_nr), snmp_integer::si_counter64);
        ifOutUcastPkts = st->register_stat(myformat("%s-ifOutUcastPkts", get_id().c_str()), myformat("1.3.6.1.2.1.2.2.1.17.%zu",    device_nr), snmp_integer::si_counter32);

	if (s)
		s->register_snmp_counters(st, get_id(), device_nr);
}
