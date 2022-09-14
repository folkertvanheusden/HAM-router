#include <map>
#include <optional>
#include <regex.h>
#include <set>

#include "filter.h"
#include "tranceiver.h"


typedef struct
{
	std::optional<filter_t>   f;
	std::vector<tranceiver *> t;
} sb_bridge_mapping_t;

typedef struct
{
	regex_t     re_from_callsign;
	regex_t     re_to_callsign;

	tranceiver *t_incoming_via;

	tranceiver *t_outgoing_via;
} sb_routing_mapping_t;

class switchboard
{
private:
	std::map<tranceiver *, std::vector<sb_bridge_mapping_t> > bridge_map;
	
	std::vector<sb_routing_mapping_t *> routing_map;

	std::mutex lock;

public:
	switchboard();
	virtual ~switchboard();

	void add_bridge_mapping(tranceiver *const in, tranceiver *const out, const std::optional<filter_t> & f);

	void add_routing_mapping(sb_routing_mapping_t *const m);

	transmit_error_t put_message(tranceiver *const from, const message & m, const bool continue_on_error);
};
