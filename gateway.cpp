#include <assert.h>
#include <atomic>
#include <signal.h>

#include "configuration.h"
#include "log.h"
#include "str.h"


std::atomic_bool terminate { false };

void signal_handler(int sig)
{
	terminate = true;

	fprintf(stderr, "Terminating...\n");

	signal(sig, SIG_IGN);
}

void process(configuration *const cfg, work_queue_t *const w)
{
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

		log(LL_DEBUG_VERBOSE, "Forwarding message from %s: %s", m.value().source.c_str(), dump_replace(m.value().message, m.value().s).c_str());

		transmit_error_t rc = cfg->get_switchboard()->put_message(t_has_work, m.value().message, m.value().s, true);

		if (rc != TE_ok)
			log(LL_INFO, "Switchboard indicated error during put_message: %d", rc);

		free(m.value().message);
	}
}

int main(int argc, char *argv[])
{
	setlogfile("gateway.log", LL_DEBUG_VERBOSE);

	signal(SIGINT, signal_handler);

	work_queue_t  w;

	configuration cfg(argc == 2 ? argv[1] : "gateway.cfg", &w);

	process(&cfg, &w);

	fprintf(stderr, "Terminating\n");

	return 0;
}
