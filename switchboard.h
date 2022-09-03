#include <map>
#include <set>

#include "filter.h"
#include "tranceiver.h"


class switchboard
{
private:
	std::map<tranceiver *, std::pair<std::set<tranceiver *>, filter *> > map;

	std::mutex lock;

public:
	switchboard();
	virtual ~switchboard();

	void add_mapping   (tranceiver *const in, tranceiver *const out, filter *const f);
	void remove_mapping(tranceiver *const in, tranceiver *const out);

	transmit_error_t put_message(tranceiver *const from, const message & m, const bool continue_on_error);
};
