#include "tranceiver.h"

tranceiver::tranceiver(const std::string & id, const seen_t & s_pars) :
	id(id),
	s(new seen(s_pars))
{
}

tranceiver::~tranceiver()
{
	terminate = true;

	th->join();
	delete th;

	delete s;
}

void tranceiver::queue_message(const message_t & m)
{
	std::unique_lock<std::mutex> lck(incoming_lock);

	incoming.push(m);

	incoming_cv.notify_all();
}

bool tranceiver::peek()
{
	std::unique_lock<std::mutex> lck(incoming_lock);

	return incoming.empty() == false;
}

message_t tranceiver::get_message()
{
	std::unique_lock<std::mutex> lck(incoming_lock);

	while(incoming.empty())
		incoming_cv.wait(lck);

	auto rc = incoming.front();
	incoming.pop();

	return rc;
}

transmit_error_t tranceiver::put_message(const uint8_t *const p, const size_t size)
{
	if (s->check(p, size))
		return put_message_low(p, size);

	return TE_ratelimiting;
}

void tranceiver::operator()()
{
}
