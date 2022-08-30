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

#include "seen.h"
#include "stats.h"
#include "work.h"


typedef enum { TE_ok, TE_hardware, TE_ratelimiting } transmit_error_t;

typedef struct {
	struct timeval tv;

	std::string    source;

	bool           from_rf;   // did it come from electromagnetic waves?
	int            air_time;  // in milliseconds

	uint8_t       *message;
	size_t         s;
} message_t;

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
	work_queue_t      *const w  { nullptr };

	seen              *const s  { nullptr };

	std::condition_variable incoming_cv;
	std::mutex              incoming_lock;
	std::queue<message_t>   incoming;

	std::thread      *th        { nullptr };

	std::atomic_bool  terminate { false   };

	virtual transmit_error_t put_message_low(const uint8_t *const p, const size_t s) = 0;

public:
	tranceiver(const std::string & id, seen *const s, work_queue_t *const w);
	virtual ~tranceiver();

	std::string get_id() const { return id; }
	virtual std::string get_type_name() const { return "base class"; }

	void register_snmp_counters(stats *const s, const int device_nr);

	transmit_error_t queue_incoming_message(const message_t & m);

	bool peek();

	std::optional<message_t> get_message();

	transmit_error_t put_message(const uint8_t *const p, const size_t s);

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w);

	virtual void operator()() = 0;
};
