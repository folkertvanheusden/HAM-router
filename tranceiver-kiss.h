#include "tranceiver.h"


class tranceiver_kiss : public tranceiver
{
private:
	std::mutex lock;
	int        fd   { -1 };

	bool recv_mkiss(unsigned char **p, int *len, bool verbose);

	bool send_mkiss(int channel, const unsigned char *p, const int len);

protected:
	transmit_error_t put_message_low(const uint8_t *const p, const size_t s);

public:
	tranceiver_kiss(const std::string & id, seen *const s, const std::string & callsign, const std::string & if_up);
	virtual ~tranceiver_kiss();

	static tranceiver *instantiate(const libconfig::Setting & node);

	void operator()();
};
