#include "dissect-packet.h"
#include "error.h"
#include "hashing.h"
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


tranceiver::tranceiver(const std::string & id, seen *const s, work_queue_t *const w, gps_connector *const gps) :
	id(id),
	w(w),
	s(s),
	gps(gps)
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

transmit_error_t tranceiver::queue_incoming_message(const message & m_in)
{
	message copy { m_in };

	auto content = copy.get_content();

	stats_add_counter(ifInOctets,   content.second);
	stats_add_counter(ifHCInOctets, content.second);
	stats_inc_counter(ifInUcastPkts);

	bool     ok   = true;
	uint32_t hash = 0;

	// check if s was allocated because e.g. the beacon module does
	// not allocate a seen object
	if (s) {
		auto ratelimit_rc = s->check(content.first, content.second);

		ok   = ratelimit_rc.first;

		hash = ratelimit_rc.second;
	}
	else {
		hash = calc_crc32(content.first, content.second);
	}

	std::map<std::string, db_record_data> meta;

	meta.insert({ "pkt-crc", db_record_gen(myformat("%08x", hash)) });

	copy.set_meta(meta);

	if (ok == false) {
		mlog(LL_DEBUG, copy, "queue_incoming_message", "dropped because of duplicates rate limiting");

		return TE_ratelimiting;
	}

	// push to incoming queue of this tranceiver (TODO: empty when not consuming (eg beacon))
	{
		auto meta2 = dissect_packet(content.first, content.second);

		if (meta2.has_value()) {
			if (meta2.value().find("latitude") != meta2.value().end() && meta2.value().find("longitude") != meta2.value().end()) {
				std::optional<position_t> position = gps->get_position();

				if (position.has_value()) {
					double cur_lat = meta2.value().find("latitude")->second.d_value;
					double cur_lng = meta2.value().find("longitude")->second.d_value;

					double distance = calcGPSDistance(cur_lat, cur_lng, position.value().latitude, position.value().longitude);

					meta2.value().insert({ "distance", myformat("%.2f", distance) });
				}
			}

			copy.set_meta(meta2.value());
		}

		std::unique_lock<std::mutex> lck(incoming_lock);

		incoming.push(copy);

		incoming_cv.notify_all();

		mlog(LL_DEBUG, copy, "queue_incoming_message", "message queued");
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

tranceiver *tranceiver::instantiate(const libconfig::Setting & node, work_queue_t *const w, gps_connector *const gps, stats *const st, const size_t device_nr, ws_global_context_t *const ws, const std::vector<tranceiver *> & tranceivers, std::map<std::string, filter *> & filters)
{
	std::string type = node.lookup("type").c_str();

	tranceiver *t = nullptr;

	if (type == "aprs-si") {
		t = tranceiver_aprs_si::instantiate(node, w, gps, st, device_nr);
	}
	else if (type == "kiss-kernel") {
		t = tranceiver_kiss_kernel::instantiate(node, w, gps);
	}
	else if (type == "kiss-tty") {
		t = tranceiver_kiss_tty::instantiate(node, w, gps);
	}
	else if (type == "lora-sx1278") {
		t = tranceiver_lora_sx1278::instantiate(node, w, gps, st, device_nr);
	}
	else if (type == "axudp") {
		t = tranceiver_axudp::instantiate(node, w, gps, filters);
	}
	else if (type == "beacon") {
		t = tranceiver_beacon::instantiate(node, w, gps);
	}
	else if (type == "database") {
		t = tranceiver_db::instantiate(node, w, gps);
	}
	else if (type == "mqtt") {
		t = tranceiver_mqtt::instantiate(node, w, gps);
	}
	else if (type == "http-ws") {
		t = tranceiver_ws::instantiate(node, w, gps, st, tranceivers);
	}
	else {
		error_exit(false, "(line %d): \"%s\" is an unknown tranceiver type", node.getSourceLine(), type.c_str());
	}

	return t;
}

void tranceiver::register_snmp_counters(stats *const st, const size_t device_nr)
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

void tranceiver::log(const int llevel, const std::string & str)
{
	::log(llevel, (get_type_name() + "(" + get_id() + "): " + str).c_str());
}

void tranceiver::mlog(const int llevel, const message & m, const std::string & where, const std::string & str)
{
	auto meta = m.get_meta();

	auto it   = meta.find("pkt-crc");

	auto crc  = it != meta.end() ? it->second.s_value : std::string("-");

	::log(llevel, (get_type_name() + "(" + get_id() + "|" + where + ")[" + m.get_id_short() + "|" + crc + "]: " + str).c_str());
}

void tranceiver::llog(const int llevel, const libconfig::Setting & node, const std::string & str)
{
	::log(llevel, (get_type_name() + "(" + get_id() + ")@" + myformat("%u", node.getSourceLine()) + ": " + str).c_str());
}
