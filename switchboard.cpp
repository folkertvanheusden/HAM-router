#include "log.h"
#include "switchboard.h"


switchboard::switchboard()
{
}

switchboard::~switchboard()
{
	for(auto & mapping : routing_map)
		delete mapping;
}

void switchboard::add_bridge_mapping(tranceiver *const in, tranceiver *const out, filter *const f)
{
	if (in == out)
		return;

	std::unique_lock<std::mutex> lck(lock);

	auto it = bridge_map.find(in);

	if (it == bridge_map.end()) {
		sb_bridge_mapping_t mapping { f, { out } };

		bridge_map.insert({ in, { mapping } });
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

void switchboard::add_routing_mapping(sb_routing_mapping_t *const m)
{
	std::unique_lock<std::mutex> lck(lock);

	routing_map.push_back(m);
}

transmit_error_t switchboard::put_message(tranceiver *const from, const message & m, const bool continue_on_error)
{
	std::unique_lock<std::mutex> lck(lock);  // TODO: r/w lock

	/* first process bridge mapping(s) */
	auto it = bridge_map.find(from);

	if (it == bridge_map.end()) {
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


	/* second, process routing mapping(s) */

	for(auto & mapping : routing_map) {
		auto meta = m.get_meta();

		// check from
		auto it   = meta.find("from");
		if (it == meta.end())
			continue;

		int rc    = regexec(&mapping->re_from_callsign, it->second.s_value.c_str(), 0, nullptr, 0);

		if (rc == REG_NOMATCH)
			continue;

		if (rc != 0) {
			log_regexp_error(rc, &mapping->re_from_callsign, "put_message: regexec for from-callsign");

			continue;
		}

		// check to
		it        = meta.find("to");
		if (it == meta.end())
			continue;

		rc = regexec(&mapping->re_to_callsign, it->second.s_value.c_str(), 0, nullptr, 0);

		if (rc == REG_NOMATCH)
			continue;

		if (rc != 0) {
			log_regexp_error(rc, &mapping->re_to_callsign, "put_message: regexec for to-callsign");

			continue;
		}

		// check incoming tranceiver
		if (mapping->t_incoming_via != nullptr && mapping->t_incoming_via != m.get_source())
			continue;

		// all is fine, put in outgoing tranceiver's queue
		mapping->t_outgoing_via->mlog(LL_DEBUG_VERBOSE, m, "put_message", "Forwarding to " + mapping->t_outgoing_via->get_id());

		forwarded = true;

		transmit_error_t t_rc = mapping->t_outgoing_via->put_message(m);

		if (t_rc != TE_ok && continue_on_error == false)
			return t_rc;
	}

	if (!forwarded) {
		log(LL_DEBUG, "NOT forwarding message %s (due to filtering or no bridge/routing match)", m.get_id_short().c_str());

		return TE_filter;
	}

	return TE_ok;
}
