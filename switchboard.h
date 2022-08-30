#include <map>
#include <set>

#include "tranceiver.h"


class switchboard
{
private:
	std::map<tranceiver *, std::set<tranceiver *> > map;

	std::mutex lock;

public:
	switchboard();
	virtual ~switchboard();

	void add_mapping   (tranceiver *const in, tranceiver *const out);
	void remove_mapping(tranceiver *const in, tranceiver *const out);

	transmit_error_t put_message(tranceiver *const from, const message & m, const bool continue_on_error);
};
