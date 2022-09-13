#pragma once

#include <atomic>
#include <condition_variable>
#include <libconfig.h++>
#include <mutex>
#include <optional>
#include <queue>
#include <stdint.h>
#include <string>
#include <thread>

#include "buffer.h"
#include "filter.h"
#include "gps.h"
#include "message.h"
#include "seen.h"
#include "stats.h"
#include "websockets.h"
#include "work.h"


typedef enum { TE_ok, TE_hardware, TE_ratelimiting, TE_filter } transmit_error_t;

class tranceiver
{
private:
	const std::string  id;

	uint64_t           snmp_dummy     { 0           };
        uint64_t          *ifInOctets     { &snmp_dummy };
        uint64_t          *ifHCInOctets   { &snmp_dummy };
        uint64_t          *ifInUcastPkts  { &snmp_dummy };
        uint64_t          *ifOutOctets    { &snmp_dummy };
        uint64_t          *ifHCOutOctets  { &snmp_dummy };
        uint64_t          *ifOutUcastPkts { &snmp_dummy };

protected:
	work_queue_t      *const w   { nullptr };

	seen              *const s   { nullptr };

	gps_connector     *const gps { nullptr };

	std::condition_variable incoming_cv;
	std::mutex              incoming_lock;
	std::queue<message>     incoming;

	std::thread      *th         { nullptr };

	std::atomic_bool  terminate  { false   };

	virtual transmit_error_t put_message_low(const message & m) = 0;

public:
	tranceiver(const std::string & id, seen *const s, work_queue_t *const w, gps_connector *const gps);
	virtual ~tranceiver();

	void stop();

	std::string get_id() const { return id; }
	virtual std::string get_type_name() const = 0;

	void register_snmp_counters(stats *const s, const size_t device_nr);

	transmit_error_t queue_incoming_message(const message & m);

	bool peek();

	void log(const int llevel, const std::string & str);
	void llog(const int llevel, const libconfig::Setting & node, const std::string & str);
	void mlog(const int llevel, const message & m, const std::string & where, const std::string & str);

	std::optional<message> get_message();

	transmit_error_t       put_message(const message & m);

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w, gps_connector *const gps, stats *const st, const size_t device_nr, ws_global_context_t *const ws, const std::vector<tranceiver *> & tranceivers, std::map<std::string, filter *> & filters, configuration *const cfg);

	virtual void operator()() = 0;
};
