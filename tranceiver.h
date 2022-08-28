#include <atomic>
#include <condition_variable>
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
	tranceiver(const std::string & id, const seen_t & s_pars);
	virtual ~tranceiver();

	void queue_message(const message_t & m);

	bool peek();

	message_t get_message();

	transmit_error_t put_message(const uint8_t *const p, const size_t s);

	virtual void operator()() = 0;
};
