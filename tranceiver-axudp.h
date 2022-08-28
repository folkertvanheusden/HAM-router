#include <string>
#include <vector>

#include "tranceiver.h"


class tranceiver_axudp : public tranceiver
{
private:
	int                      fd                { -1    };
	const int                listen_port       { -1    };
	std::vector<std::string> destinations;
	const bool               continue_on_error { false };

protected:
	transmit_error_t put_message_low(const uint8_t *const p, const size_t s);

public:
	tranceiver_axudp(const std::string & id, seen *const s, work_queue_t *const w, const int listen_port, const std::vector<std::string> & destinations, const bool continue_on_error);
	virtual ~tranceiver_axudp();

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w);

	void operator()();
};
