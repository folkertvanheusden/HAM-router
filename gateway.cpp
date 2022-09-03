#include <assert.h>
#include <atomic>
#include <jansson.h>
#include <signal.h>

#include "configuration.h"
#include "log.h"
#include "snmp.h"
#include "stats.h"
#include "str.h"
#include "utils.h"
#include "websockets.h"


std::atomic_bool terminate { false };

void signal_handler(int sig)
{
	terminate = true;

	fprintf(stderr, "Terminating...\n");

	signal(sig, SIG_IGN);
}

void push_to_websockets(ws_global_context_t *const ws, const message & m)
{
	auto          & m        = m.get_meta();

	json_t         *jsom_out = json_object();

	json_object_set_new(jsom_out, "timestamp", json_integer(m.get_tv().tv_sec));

	json_object_set_new(jsom_out, "source",    json_string(m.get_source().c_str()));

	json_object_set_new(jsom_out, "msg-id",    json_string(m.get_id_short().c_str()));

	json_object_set_new(jsom_out, "air-time",  json_integer(m.get_air_time()));

	json_object_set_new(jsom_out, "data",      json_string(dump_replace(m.get_content().first, m.get_content().second).c_str()));

	if (m.find("from") != m.end())
		json_object_set_new(jsom_out, "from", json_string(m.at("from").c_str()));

	if (m.find("to") != m.end())
		json_object_set_new(jsom_out, "to",   json_string(m.at("to").c_str()));

	char *json = json_dumps(jsom_out, 0);

	std::string jsom_out_str = json;

	free(json);

	json_decref(jsom_out);

	push_to_websockets(ws, jsom_out_str);
}

std::thread * process(configuration *const cfg, work_queue_t *const w, snmp *const snmp_)
{
	return new std::thread([cfg, w, snmp_] {
		set_thread_name("main");

		for(;;) {
			tranceiver *t_has_work { nullptr };

			{
				std::unique_lock lck(w->work_lock);

				while(w->work_list.empty() && !terminate)
					w->work_cv.wait_for(lck, std::chrono::milliseconds(100));

				if (terminate)
					break;

				t_has_work = w->work_list.front();
				w->work_list.pop();
			}

			assert(t_has_work->peek());

			auto m = t_has_work->get_message();

			if (m.has_value() == false) {
				if (!terminate)
					log(LL_WARNING, "Tranceiver \"%s\" did not return data while it had ready-state", t_has_work->get_id().c_str());

				continue;
			}

			// Put this in a thread vvvv
			auto content = m.value().get_content();

			log(LL_DEBUG_VERBOSE, "Forwarding message from %s (%s): %s", m.value().get_source().c_str(), m.value().get_id_short().c_str(), dump_replace(content.first, content.second).c_str());

			transmit_error_t rc = cfg->get_switchboard()->put_message(t_has_work, m.value(), true);

			if (rc != TE_ok)
				log(LL_INFO, "Switchboard indicated error during put_message: %d", rc);

			push_to_websockets(cfg->get_websockets_context(), m.value());
			// Put this in a thread ^^^^
		}
	});
}

int main(int argc, char *argv[])
{
	setlogfile("gateway.log", LL_DEBUG_VERBOSE);

	signal(SIGINT, signal_handler);

	work_queue_t  w;

        snmp_data     sd;

	stats         st(8192, &sd);

	configuration cfg(argc == 2 ? argv[1] : "gateway.cfg", &w, &sd, &st);

	setlogfile(cfg.get_logfile().c_str(), LL_DEBUG_VERBOSE);

	snmp          *snmp_ = new snmp(&sd, &st, cfg.get_snmp_port());

	std::thread *th = process(&cfg, &w, snmp_);
	th->join();
	delete th;

	delete snmp_;

	unsetlogfile();

	fprintf(stderr, "Terminating\n");

	return 0;
}
