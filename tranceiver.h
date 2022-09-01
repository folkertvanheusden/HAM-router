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

#include "data.h"
#include "seen.h"
#include "stats.h"
#include "work.h"


typedef enum { TE_ok, TE_hardware, TE_ratelimiting } transmit_error_t;

class message {
private:
	const struct timeval tv;

	const std::string    source;

	const uint64_t       msg_id;

	const bool           from_rf;   // did it come from electromagnetic waves?
	const int            air_time;  // in milliseconds

	const data           d;

public:
	message(const struct timeval tv, const std::string & source, const uint64_t msg_id, const bool from_rf, const int air_time, const uint8_t *const data, const size_t size);

	message(const message & m);

	virtual ~message();

	struct timeval get_tv()         const { return tv;       }

	std::string    get_source()     const { return source;   }

	uint64_t       get_msg_id()     const { return msg_id;   }

	bool           get_is_from_rf() const { return from_rf;  }

	int            get_air_time()   const { return air_time; }

	std::pair<const uint8_t *, size_t> get_content() const { return d.get_content(); }

	const uint8_t *get_data()       const { return d.get_data(); }

	size_t         get_size()       const { return d.get_size(); }

	const data   & get_data_obj()   const { return d;            }

	std::string    get_id_short()   const;
};

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
	std::queue<message>     incoming;

	std::thread      *th        { nullptr };

	std::atomic_bool  terminate { false   };

	virtual transmit_error_t put_message_low(const message & m) = 0;

public:
	tranceiver(const std::string & id, seen *const s, work_queue_t *const w);
	virtual ~tranceiver();

	std::string get_id() const { return id; }
	virtual std::string get_type_name() const { return "base class"; }

	void register_snmp_counters(stats *const s, const int device_nr);

	transmit_error_t queue_incoming_message(const message & m);

	bool peek();

	std::optional<message> get_message();

	transmit_error_t       put_message(const message & m);

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w, stats *const st, const int device_nr);

	virtual void operator()() = 0;
};
