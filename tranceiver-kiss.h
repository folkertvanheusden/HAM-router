#include "tranceiver.h"


class tranceiver_kiss : public tranceiver
{
private:
	std::mutex lock;
	int        fd   { -1 };

	bool recv_mkiss(unsigned char **p, int *len, bool verbose);

	bool send_mkiss(int channel, const unsigned char *p, const int len);

protected:
	transmit_error_t put_message_low(const message & m) override;

public:
	tranceiver_kiss(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos, const std::string & callsign, const std::string & if_up);
	virtual ~tranceiver_kiss();

	std::string get_type_name() const override { return "KISS"; }

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w, const position_t & pos);

	void operator()() override;
};
