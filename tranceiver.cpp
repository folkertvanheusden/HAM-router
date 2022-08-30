#include "error.h"
#include "log.h"
#include "str.h"
#include "time.h"
#include "utils.h"
#include "tranceiver.h"
#include "tranceiver-aprs-si.h"
#include "tranceiver-axudp.h"
#include "tranceiver-beacon.h"
#include "tranceiver-kiss.h"
#include "tranceiver-lora-sx1278.h"


message::message(const struct timeval tv, const std::string & source, const uint64_t msg_id, const bool from_rf, const int air_time, const uint8_t *const data, const size_t size) :
	tv      (tv),
	source  (source),
	msg_id  (msg_id),
	from_rf (from_rf),
	air_time(air_time),
	data(reinterpret_cast<uint8_t *>(duplicate(data, size))),
	size    (size)
{
}

message::message(const message & m) :
	tv      (m.get_tv()),
	source  (m.get_source()),
	msg_id  (m.get_msg_id()),
	from_rf (m.get_is_from_rf()),
	air_time(m.get_air_time()),
	data    (reinterpret_cast<uint8_t *>(duplicate(m.get_data(), m.get_size()))),
	size    (m.get_size())
{
}

message::~message()
{
	free(const_cast<uint8_t *>(data));
}

std::string message::get_id_short() const
{
	return myformat("%08x", msg_id);
}

tranceiver::tranceiver(const std::string & id, seen *const s, work_queue_t *const w) :
	id(id),
	w(w),
	s(s)
{
}

tranceiver::~tranceiver()
{
	delete s;
}

transmit_error_t tranceiver::queue_incoming_message(const message & m)
{
	auto content = m.get_content();

	stats_add_counter(ifInOctets,   content.second);
	stats_add_counter(ifHCInOctets, content.second);
	stats_inc_counter(ifInUcastPkts);

	// check if s was allocated because e.g. the beacon module does
	// not allocate a seen object
	if (s && s->check(content.first, content.second) == false) {
		log(LL_DEBUG, "tranceiver::queue_incoming_message(%s: %s): dropped because of rate limiting", id.c_str(), m.get_id_short().c_str());

		return TE_ratelimiting;
	}

	{
		std::unique_lock<std::mutex> lck(incoming_lock);

		incoming.push(m);

		incoming_cv.notify_all();
	}

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
	size_t size = m.get_size();

	stats_add_counter(ifOutOctets,   size);
	stats_add_counter(ifHCOutOctets, size);
	stats_inc_counter(ifOutUcastPkts);

	return put_message_low(m);
}

tranceiver *tranceiver::instantiate(const libconfig::Setting & node, work_queue_t *const w)
{
	std::string type = node.lookup("type").c_str();

	tranceiver *t = nullptr;

	if (type == "aprs-si") {
		t = tranceiver_aprs_si::instantiate(node, w);
	}
	else if (type == "kiss") {
		t = tranceiver_kiss::instantiate(node, w);
	}
	else if (type == "lora-sx1278") {
		t = tranceiver_lora_sx1278::instantiate(node, w);
	}
	else if (type == "axudp") {
		t = tranceiver_axudp::instantiate(node, w);
	}
	else if (type == "beacon") {
		t = tranceiver_beacon::instantiate(node, w);
	}
	else {
		error_exit(false, "\"%s\" is an unknown tranceiver type", type.c_str());
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
