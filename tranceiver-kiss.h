#include "tranceiver.h"


class tranceiver_kiss : public tranceiver
{
private:
	std::mutex lock;
	int        fd   { -1 };

	bool recv_mkiss(int fd, unsigned char **p, int *len, bool verbose);

	bool send_mkiss(int fd, int channel, const unsigned char *p, const int len);

protected:
	transmit_error_t put_message_low(const uint8_t *const p, const size_t s);

public:
	tranceiver_kiss(const std::string & id, const seen_t & s_pars, const std::string & callsign, const std::string & if_up);
	virtual ~tranceiver_kiss();

	void queue_message(const message_t & m);

	void operator()();
};
