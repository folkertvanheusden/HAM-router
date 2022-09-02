#include <vector>

#include "snmp.h"
#include "switchboard.h"
#include "tranceiver.h"
#include "webserver.h"
#include "websockets.h"


class configuration
{
private:
	std::vector<tranceiver *>  tranceivers;

	switchboard               *sb        { nullptr };

	int                        snmp_port { -1      };

	void                      *webserver { nullptr };

	db                        *d         { nullptr };

	ws_global_context_t        ws;

	position_t                 local_pos { 0       };

	std::string                logfile   { "gateway.log" };

	snmp_data_type_running_since *running_since { new snmp_data_type_running_since() };

	void load_database   (const libconfig::Setting & node_in);

	void load_general    (const libconfig::Setting & node_in);

	void load_snmp       (const libconfig::Setting & node_in, snmp_data *const sd);

	void load_switchboard(const libconfig::Setting & node);

	void load_tranceivers(const libconfig::Setting & node, work_queue_t *const w, snmp_data *const sd, stats *const st);

	void load_webserver  (const libconfig::Setting & node, stats *const st);

public:
	configuration(const std::string & file, work_queue_t *const w, snmp_data *const sd, stats *const st);
	virtual ~configuration();

	std::vector<tranceiver *> & get_tranceivers() { return tranceivers; }

	tranceiver  * find_tranceiver(const std::string & id);

	db          * get_db()                { return d;         }

	switchboard * get_switchboard() const { return sb;        }

	int           get_snmp_port() const   { return snmp_port; }

	ws_global_context_t * get_websockets_context() { return &ws; }

	position_t    get_local_pos() const   { return local_pos; }

	std::string   get_logfile() const     { return logfile;   }
};
