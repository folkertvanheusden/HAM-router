#include "tranceiver.h"


class tranceiver_aprs_si : public tranceiver
{
private:
	std::mutex  lock;
	int         fd { -1 };

	std::string aprs_user;
	std::string aprs_pass;

protected:
	transmit_error_t put_message_low(const uint8_t *const p, const size_t s);

public:
	tranceiver_aprs_si(const std::string & id, seen *const s, const std::string & aprs_user, const std::string & aprs_pass);
	virtual ~tranceiver_aprs_si();

	static tranceiver *instantiate(const libconfig::Setting & node);

	void operator()();
};
