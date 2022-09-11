#pragma once

#include <atomic>
#include <condition_variable>
#include <libconfig.h++>
#include <map>
#include <mutex>
#include <stdint.h>
#include <stdlib.h>
#include <thread>

#include "rate-limiter.h"
#include "stats.h"


typedef struct {
	int    max_per_dt;
	double dt;
	int    max_seen_elements;
} seen_t;

class seen
{
private:
	const int    max_per_dt { 0 };
	const double dt         { 0 };
	const int    max_n      { 0 };

	std::condition_variable            history_cv;
	std::map<uint32_t, rate_limiter *> history;
	std::mutex                         history_lock;

	uint64_t        *counter_hit  { nullptr };
	uint64_t        *counter_miss { nullptr };

	std::atomic_bool terminate { false   };

	std::thread     *th        { nullptr };

public:
	seen(const seen_t & pars);
	~seen();

	void stop();

	std::pair<bool, uint32_t> check(const uint8_t *const p, const size_t s);

	static seen *instantiate(const libconfig::Setting & node);

	void register_snmp_counters(stats *const st, const std::string & parent_id, const size_t device_nr);

	void operator()();
};
