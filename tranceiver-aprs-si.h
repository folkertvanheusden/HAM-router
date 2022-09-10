#include "tranceiver.h"


class tranceiver_aprs_si : public tranceiver
{
private:
	std::mutex  lock;
	int         fd { -1 };

	std::string aprs_user;
	std::string aprs_pass;

	std::string local_callsign;

	uint64_t *cnt_frame_aprs              { nullptr };
	uint64_t *cnt_frame_aprs_rate_limited { nullptr };
	uint64_t *cnt_aprs_invalid            { nullptr };

protected:
	transmit_error_t put_message_low(const message & m) override;

public:
	tranceiver_aprs_si(const std::string & id, seen *const s, work_queue_t *const w, gps_connector *const gps, const std::string & aprs_user, const std::string & aprs_pass, const std::string & local_callsign, stats *const st, int device_nr);
	virtual ~tranceiver_aprs_si();

	std::string get_type_name() const override { return "APRS-SI"; }

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w, gps_connector *const gps, stats *const st, int device_nr);

	void operator()() override;
};
