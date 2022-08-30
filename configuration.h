#include <vector>

#include "snmp.h"
#include "switchboard.h"
#include "tranceiver.h"


class configuration
{
private:
	std::vector<tranceiver *>    tranceivers;

	switchboard                 *sb        { nullptr };

	int                          snmp_port { -1      };

	snmp_data                   *sd        { nullptr };

	snmp_data_type_running_since running_since;

	void load_tranceivers(const libconfig::Setting & node, work_queue_t *const w, stats *const st);

	void load_snmp       (const libconfig::Setting & node_in);

	void load_switchboard(const libconfig::Setting & node);

public:
	configuration(const std::string & file, work_queue_t *const w, snmp_data *const sd, stats *const st);
	virtual ~configuration();

	std::vector<tranceiver *> & get_tranceivers() { return tranceivers; }

	tranceiver  * find_tranceiver(const std::string & id);

	switchboard * get_switchboard() const { return sb;        }

	int           get_snmp_port() const   { return snmp_port; }
};
