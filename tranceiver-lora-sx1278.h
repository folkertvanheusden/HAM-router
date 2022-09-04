#include <stdbool.h>

extern "C" {
#include "LoRa.h"
}

#include "stats.h"
#include "tranceiver.h"


class tranceiver_lora_sx1278 : public tranceiver
{
private:
	std::mutex lock;
	LoRa_ctl   modem;
	const bool digipeater;

	uint64_t  *invalid_pkts { nullptr };
	uint64_t  *valid_pkts   { nullptr };

protected:
	transmit_error_t put_message_low(const message & m) override;

public:
	tranceiver_lora_sx1278(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos, const int dio0_pin, const int reset_pin, const bool digipeater, stats *const st, const int dev_nr);
	virtual ~tranceiver_lora_sx1278();

	void count_packets(const bool valid);

	std::string get_type_name() const override { return "LoRa-SX1278"; }

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w, const position_t & pos, stats *const st, const int dev_nr);

	bool is_digipeater() const { return digipeater; }

	void operator()() override;
};
