#include "log.h"
#include "switchboard.h"


switchboard::switchboard()
{
}

switchboard::~switchboard()
{
}

void switchboard::add_mapping(tranceiver *const in, tranceiver *const out)
{
	if (in == out)
		return;

	std::unique_lock<std::mutex> lck(lock);

	auto it = map.find(in);

	if (it == map.end()) {
		map.insert({ in, { out } });
	}
	else {
		it->second.insert(out);
	}
}

void switchboard::remove_mapping(tranceiver *const in, tranceiver *const out)
{
	std::unique_lock<std::mutex> lck(lock);

	auto it = map.find(in);

	if (it != map.end())
		it->second.erase(out);
}

transmit_error_t switchboard::put_message(tranceiver *const from, const uint8_t *const p, const size_t size, const bool continue_on_error)
{
	std::unique_lock<std::mutex> lck(lock);  // TODO: r/w lock

	auto it = map.find(from);

	if (it == map.end())
		return TE_hardware;

	log(LL_DEBUG, "Forwarding to %zu tranceivers", it->second.size());

	// TODO in a thread; copy data then!
	for(auto t : it->second) {
		log(LL_DEBUG_VERBOSE, "Forwarding to: %s", t->get_id().c_str());

		transmit_error_t rc = t->put_message(p, size);

		if (rc != TE_ok && continue_on_error == false)
			return rc;
	}

	log(LL_DEBUG_VERBOSE, "Forward ok");

	return TE_ok;
}
