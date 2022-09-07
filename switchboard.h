#include <map>
#include <set>

#include "filter.h"
#include "tranceiver.h"


typedef struct
{
	filter *f;
	std::vector<tranceiver *> t;
} sb_mapping_t;

class switchboard
{
private:
	std::map<tranceiver *, std::vector<sb_mapping_t> > map;

	std::mutex lock;

public:
	switchboard();
	virtual ~switchboard();

	void add_mapping(tranceiver *const in, tranceiver *const out, filter *const f);

	transmit_error_t put_message(tranceiver *const from, const message & m, const bool continue_on_error);
};
