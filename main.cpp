#include <assert.h>
#include <atomic>
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

std::thread * process(configuration *const cfg, work_queue_t *const w, snmp *const snmp_)
{
	return new std::thread([cfg, w, snmp_] {
		set_thread_name("main");

		seen *s = cfg->get_global_repetition_filter();

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

			if (s) {
				auto ratelimit_rc = s->check(content.first, content.second);

				if (ratelimit_rc.first == false) {
					log(LL_DEBUG, "main(%s): dropped because of duplicates rate limiting", m.value().get_id_short().c_str());

					continue;
				}
			}

			log(LL_DEBUG_VERBOSE, "Forwarding message from %s (%s): %s", m.value().get_source().c_str(), m.value().get_id_short().c_str(), dump_replace(content.first, content.second).c_str());

			transmit_error_t rc = cfg->get_switchboard()->put_message(t_has_work, m.value(), true);

			if (rc != TE_ok)
				log(LL_INFO, "Switchboard indicated error during put_message: %d", rc);
			// Put this in a thread ^^^^
		}
	});
}

int main(int argc, char *argv[])
{
	setlogfile("ham-router.log", LL_DEBUG_VERBOSE);

	signal(SIGINT, signal_handler);

	work_queue_t  w;

        snmp_data     sd;

	stats         st(8192, &sd);

	configuration cfg(argc == 2 ? argv[1] : "ham-router.cfg", &w, &sd, &st);

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
