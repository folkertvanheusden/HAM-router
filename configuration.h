#include <vector>

#include "switchboard.h"
#include "tranceiver.h"


class configuration
{
private:
	std::vector<tranceiver *> tranceivers;

	switchboard              *sb = nullptr;

	void load_tranceivers(const libconfig::Setting & node, work_queue_t *const w);

	void load_switchboard(const libconfig::Setting & node);

public:
	configuration(const std::string & file, work_queue_t *const w);
	virtual ~configuration();

	std::vector<tranceiver *> & get_tranceivers() { return tranceivers; }

	tranceiver * find_tranceiver(const std::string & id);

	switchboard * get_switchboard() const { return sb; }
};
