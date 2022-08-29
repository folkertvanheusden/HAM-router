#include <time.h>
#include <vector>

#include "error.h"
#include "hashing.h"
#include "seen.h"
#include "time.h"


seen::seen(const seen_t & pars) :
	max_per_dt(pars.max_per_dt),
	dt(pars.dt),
	max_n(pars.max_seen_elements)
{
	th = new std::thread(std::ref(*this));
}

seen::~seen()
{
	terminate = true;

	{
		std::unique_lock<std::mutex> lck(history_lock);
		history_cv.notify_one();
	}

	th->join();
	delete th;

	for(auto it : history)
		delete it.second;
}

bool seen::check(const uint8_t *const p, const size_t s)
{
	uint32_t hash = calc_crc32(p, s);

	std::unique_lock<std::mutex> lck(history_lock);

	auto it = history.find(hash);

	if (it == history.end()) {
		rate_limiter *r = new rate_limiter(max_per_dt, dt);

		history.insert({ hash, r });

		history_cv.notify_one();

		return r->check();
	}

	return it->second->check();
}

void seen::operator()()
{
	while(!terminate) {
		std::unique_lock<std::mutex> lck(history_lock);

		while(history.size() < size_t(max_n) && terminate == false)
			history_cv.wait_for(lck, std::chrono::milliseconds(END_CHECK_INTERVAL_ms));

		std::vector<std::pair<uint32_t, time_t> > map;

		for(auto it : history)
			map.push_back({ it.first, it.second->get_last_ts() });

		std::sort(map.begin(), map.end(),
				[](const std::pair<uint32_t, time_t> & a, const std::pair<uint32_t, time_t> & b) -> bool
				{
					return a.second > b.second;
				});

		int lower_bound = history.size() - max_n + max_n / 10;

		for(int i=0; i<lower_bound; i++) {
			uint32_t cur_hash = map.at(i).first;

			delete history.find(cur_hash)->second;

			history.erase(cur_hash);
		}
	}
}

seen *seen::instantiate(const libconfig::Setting & node_in)
{
	seen_t pars_seen { 0 };

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "max-per-interval")
			pars_seen.max_per_dt        = node_in.lookup(type);
		else if (type == "interval-duration")
			pars_seen.dt                = int(node_in.lookup(type));
		else if (type == "max-n-elements")
			pars_seen.max_seen_elements = node_in.lookup(type);
		else
			error_exit(false, "setting \"%s\" is now known", type.c_str());
        }

	return new seen(pars_seen);
}
