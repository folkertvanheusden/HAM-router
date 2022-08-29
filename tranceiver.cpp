#include "error.h"
#include "log.h"
#include "time.h"
#include "tranceiver.h"
#include "tranceiver-aprs-si.h"
#include "tranceiver-axudp.h"
#include "tranceiver-beacon.h"
#include "tranceiver-kiss.h"
#include "tranceiver-lora-sx1278.h"


tranceiver::tranceiver(const std::string & id, seen *const s, work_queue_t *const w) :
	id(id),
	w(w),
	s(s)
{
}

tranceiver::~tranceiver()
{
	while(incoming.empty() == false) {
		free(incoming.front().message);

		incoming.pop();
	}

	delete s;
}

transmit_error_t tranceiver::queue_incoming_message(const message_t & m)
{
	// check if s was allocated because e.g. the beacon module does
	// not allocate a seen object
	if (s && s->check(m.message, m.s)) {
		log(LL_DEBUG, "tranceiver::queue_incoming_message(%s): dropped because of rate limiting", id.c_str());

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

std::optional<message_t> tranceiver::get_message()
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

transmit_error_t tranceiver::put_message(const uint8_t *const p, const size_t size)
{
	return put_message_low(p, size);
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
