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
		sb_mapping_t mapping { f, { out } };

		map.insert({ in, { mapping } });
	}
	else {
		for(auto & mapping : it->second) {
			if (mapping.f == f) {
				mapping.t.push_back(out);
				return;
			}
		}
	}
}

transmit_error_t switchboard::put_message(tranceiver *const from, const message & m, const bool continue_on_error)
{
	std::unique_lock<std::mutex> lck(lock);  // TODO: r/w lock

	auto it = map.find(from);

	if (it == map.end()) {
		log(LL_DEBUG, "put_message: tranceiver %s is not mapped", from->get_id().c_str());

		return TE_hardware;
	}

	bool forwarded = false;

	for(auto & target_filters_pair : it->second) {
		if (target_filters_pair.f == nullptr || target_filters_pair.f->check(m)) {
			log(LL_DEBUG, "Forwarding %s to %zu tranceivers", m.get_id_short().c_str(), target_filters_pair.t.size());

			for(auto t : target_filters_pair.t) {
				t->mlog(LL_DEBUG_VERBOSE, m, "put_message", "Forwarding to " + t->get_id());

				transmit_error_t rc = t->put_message(m);

				if (rc != TE_ok && continue_on_error == false)
					return rc;
			}

			forwarded = true;
		}
	}

	if (!forwarded) {
		log(LL_DEBUG, "NOT forwarding message %s: filtered", m.get_id_short().c_str());

		return TE_filter;
	}

	return TE_ok;
}
