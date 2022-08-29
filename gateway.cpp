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
}

void process(configuration *const cfg, work_queue_t *const w)
{
	for(;;) {
		std::unique_lock lck(w->work_lock);

		while(w->work_list.empty() && !terminate)
			w->work_cv.wait(lck);

		if (terminate)
			break;

		tranceiver *const t_has_work = w->work_list.front();
		w->work_list.pop();

		assert(t_has_work->peek());

		message_t m = t_has_work->get_message();

		log(LL_DEBUG_VERBOSE, "Forwarding message %s", dump_replace(m.message, m.s).c_str());

		printf("%d\n", cfg->get_switchboard()->put_message(t_has_work, m.message, m.s, true));

		free(m.message);
	}
}

int main(int argc, char *argv[])
{
	setlogfile("gateway.log", LL_DEBUG_VERBOSE);

	signal(SIGINT, signal_handler);

	work_queue_t  w;

	configuration cfg("gateway.cfg", &w);

	process(&cfg, &w);

	return 0;
}
