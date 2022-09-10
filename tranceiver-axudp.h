#include <string>
#include <vector>

#include "filter.h"
#include "tranceiver.h"


class tranceiver_axudp : public tranceiver
{
private:
	int        fd                { -1    };
	const int  listen_port       { -1    };
	std::vector<std::pair<std::string, filter *> > peers;
	const bool continue_on_error { false };
	const bool distribute        { false };

	transmit_error_t send_to_other_axudp_targets(const message & m, const std::string & came_from);

protected:
	transmit_error_t put_message_low(const message & m) override;

public:
	tranceiver_axudp(const std::string & id, seen *const s, work_queue_t *const w, gps_connector *const gps, const int listen_port, const std::vector<std::pair<std::string, filter *> > & filters, const bool continue_on_error, const bool distribute);
	virtual ~tranceiver_axudp();

	std::string get_type_name() const override { return "AXUDP"; }

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w, gps_connector *const gps, const std::map<std::string, filter *> & filters);

	void operator()() override;
};
