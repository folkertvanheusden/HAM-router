#include <string>
#include <vector>

#include "tranceiver.h"


class tranceiver_beacon : public tranceiver
{
private:
	const std::string beacon_text;
	const int         beacon_interval;

protected:
	transmit_error_t put_message_low(const uint8_t *const p, const size_t s);

public:
	tranceiver_beacon(const std::string & id, seen *const s, work_queue_t *const w, const std::string & beacon_text, const int beacon_interval);
	virtual ~tranceiver_beacon();

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w);

	void operator()();
};
