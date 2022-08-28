#pragma once
#include <atomic>
#include <condition_variable>
#include <libconfig.h++>
#include <mutex>
#include <queue>
#include <stdint.h>
#include <string>
#include <thread>

#include "seen.h"


typedef enum { TE_ok, TE_hardware, TE_ratelimiting } transmit_error_t;

typedef struct {
	struct timeval tv;

	uint8_t       *message;
	size_t         s;
} message_t;

class tranceiver
{
private:
	const std::string id;

	std::thread *const th { nullptr };

protected:
	std::atomic_bool terminate { false };

	seen        *const s  { nullptr };

	std::condition_variable incoming_cv;
	std::mutex              incoming_lock;
	std::queue<message_t>   incoming;

	virtual transmit_error_t put_message_low(const uint8_t *const p, const size_t s) = 0;

public:
	tranceiver(const std::string & id, seen *const s);
	virtual ~tranceiver();

	std::string get_id() const { return id; }

	void queue_incoming_message(const message_t & m);

	bool peek();

	message_t get_message();

	transmit_error_t put_message(const uint8_t *const p, const size_t s);

	static tranceiver *instantiate(const libconfig::Setting & node);

	virtual void operator()() = 0;
};
