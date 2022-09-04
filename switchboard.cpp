#include "log.h"
#include "switchboard.h"


switchboard::switchboard()
{
}

switchboard::~switchboard()
{
}

void switchboard::add_mapping(tranceiver *const in, tranceiver *const out, filter *const f)
{
	if (in == out)
		return;

	std::unique_lock<std::mutex> lck(lock);

	auto it = map.find(in);

	if (it == map.end()) {
		map.insert({ in, { { out }, f } });
	}
	else {
		it->second.first.insert(out);
	}
}

void switchboard::remove_mapping(tranceiver *const in, tranceiver *const out)
{
	std::unique_lock<std::mutex> lck(lock);

	auto it = map.find(in);

	if (it != map.end())
		it->second.first.erase(out);
}

transmit_error_t switchboard::put_message(tranceiver *const from, const message & m, const bool continue_on_error)
{
	std::unique_lock<std::mutex> lck(lock);  // TODO: r/w lock

	auto it = map.find(from);

	if (it == map.end())
		return TE_hardware;

	if (it->second.second) {
		if (it->second.second->check(m)) {
			log(LL_DEBUG, "NOT forwarding message %s: filtered", m.get_id_short().c_str());

			return TE_filter;
		}
	}

	log(LL_DEBUG, "Forwarding %s to %zu tranceivers", m.get_id_short().c_str(), it->second.first.size());

	for(auto t : it->second.first) {
		log(LL_DEBUG_VERBOSE, "Forwarding %s to: %s", m.get_id_short().c_str(), t->get_id().c_str());

		transmit_error_t rc = t->put_message(m);

		if (rc != TE_ok && continue_on_error == false)
			return rc;
	}

	log(LL_DEBUG_VERBOSE, "Forward of %s ok", m.get_id_short().c_str());

	return TE_ok;
}
