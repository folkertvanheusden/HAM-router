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
		return TE_ok;

	// TODO in a thread
	for(auto t : it->second) {
		transmit_error_t rc = t->put_message(p, size);

		if (rc != TE_ok && continue_on_error == false)
			return rc;
	}

	return TE_ok;
}
