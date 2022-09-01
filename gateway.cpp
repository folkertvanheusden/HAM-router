#include "config.h"
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

void insert_into_database(db *const d, const message & m)
{
	db_record record(m.get_tv());

	db_record_insert(&record, "raw-data", m.get_buffer());

	db_record_insert(&record, "source", m.get_source());

	db_record_insert(&record, "msg-id", int64_t(m.get_msg_id()));

	if (m.get_is_from_rf())
		db_record_insert(&record, "air-time", double(m.get_air_time()));

	d->insert(record);
}

void push_to_websockets(ws_global_context_t *const ws, const message & m)
{
	json_t         *meta = json_object();

	json_object_set(meta, "timestamp", json_integer(m.get_tv().tv_sec));

	json_object_set(meta, "source",    json_string(m.get_source().c_str()));

	json_object_set(meta, "msg-id",    json_string(m.get_id_short().c_str()));

	json_object_set(meta, "air-time",  json_integer(m.get_air_time()));

	json_object_set(meta, "data",      json_string(dump_replace(m.get_content().first, m.get_content().second).c_str()));

	std::string meta_str = json_dumps(meta, 0);

	json_decref(meta);

	push_to_websockets(ws, meta_str);
}

std::thread * process(configuration *const cfg, work_queue_t *const w, snmp *const snmp_)
{
	return new std::thread([cfg, w, snmp_] {
		set_thread_name("main");

		for(;;) {
			std::unique_lock lck(w->work_lock);

			while(w->work_list.empty() && !terminate)
				w->work_cv.wait_for(lck, std::chrono::milliseconds(100));

			if (terminate)
				break;

			tranceiver *const t_has_work = w->work_list.front();
			w->work_list.pop();

			assert(t_has_work->peek());

			auto m = t_has_work->get_message();

			if (m.has_value() == false) {
				if (!terminate)
					log(LL_WARNING, "Tranceiver \"%s\" did not return data while it had ready-state", t_has_work->get_id().c_str());

				continue;
			}

			auto content = m.value().get_content();

			log(LL_DEBUG_VERBOSE, "Forwarding message from %s (%s): %s", m.value().get_source().c_str(), m.value().get_id_short().c_str(), dump_replace(content.first, content.second).c_str());

			transmit_error_t rc = cfg->get_switchboard()->put_message(t_has_work, m.value(), true);

			if (rc != TE_ok)
				log(LL_INFO, "Switchboard indicated error during put_message: %d", rc);

			db *d = cfg->get_db();

#if LIBMONGOCXX_FOUND == 1
			if (d)
				insert_into_database(d, m.value());
#endif

			push_to_websockets(cfg->get_websockets_context(), m.value());
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

	snmp          *snmp_ = new snmp(&sd, &st, cfg.get_snmp_port());

	std::thread *th = process(&cfg, &w, snmp_);
	th->join();
	delete th;

	delete snmp_;

	unsetlogfile();

	fprintf(stderr, "Terminating\n");

	return 0;
}
