#include <assert.h>
#include <atomic>

#include "configuration.h"
#include "log.h"
#include "str.h"


std::atomic_bool terminate { false };

void process(configuration *const cfg, work_queue_t *const w)
{
	while(!terminate) {
		std::unique_lock lck(w->work_lock);

		while(w->work_list.empty())
			w->work_cv.wait(lck);

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
	setlogfile("/var/log/gateway.log", LL_DEBUG_VERBOSE);

	work_queue_t  w;

	configuration cfg("gateway.cfg", &w);

	process(&cfg, &w);

	return 0;
}
