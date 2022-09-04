#pragma once

#include "tranceiver.h"


class tranceiver_kiss : public tranceiver
{
protected:
	std::mutex lock;
	int        fd   { -1 };

	bool recv_mkiss(unsigned char **p, int *len, bool verbose);

	bool send_mkiss(int channel, const unsigned char *p, const int len);

	transmit_error_t put_message_low(const message & m) override;

public:
	tranceiver_kiss(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos);
	virtual ~tranceiver_kiss();

	virtual std::string get_type_name() const override { return "KISS-base"; }

	void operator()() override;
};
