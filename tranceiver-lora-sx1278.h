#include <stdbool.h>

extern "C" {
#include "LoRa.h"
}

#include "tranceiver.h"


class tranceiver_lora_sx1278 : public tranceiver
{
private:
	std::mutex  lock;
	LoRa_ctl    modem;

protected:
	transmit_error_t put_message_low(const uint8_t *const p, const size_t s);

public:
	tranceiver_lora_sx1278(const std::string & id, seen *const s, work_queue_t *const w, const int dio0_pin, const int reset_pin);
	virtual ~tranceiver_lora_sx1278();

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w);

	void operator()();
};
