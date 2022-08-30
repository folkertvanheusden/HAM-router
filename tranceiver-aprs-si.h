#include "tranceiver.h"


class tranceiver_aprs_si : public tranceiver
{
private:
	std::mutex  lock;
	int         fd { -1 };

	std::string aprs_user;
	std::string aprs_pass;

protected:
	transmit_error_t put_message_low(const message & m) override;

public:
	tranceiver_aprs_si(const std::string & id, seen *const s, work_queue_t *const w, const std::string & aprs_user, const std::string & aprs_pass);
	virtual ~tranceiver_aprs_si();

	std::string get_type_name() const override { return "APRS-SI"; }

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w);

	void operator()() override;
};
