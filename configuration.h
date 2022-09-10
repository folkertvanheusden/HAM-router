#include <map>
#include <vector>

#include "filter.h"
#include "gps.h"
#include "seen.h"
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

	std::string                logfile   { "gateway.log" };

	gps_connector             *gps       { nullptr };

	std::map<std::string, filter *> filters;

	seen                      *global_repetition_filter { nullptr };

	snmp_data_type_running_since *running_since { new snmp_data_type_running_since() };

	void load_database   (const libconfig::Setting & node_in);

	void load_filters    (const libconfig::Setting & node_in);

	void load_general    (const libconfig::Setting & node_in);

	void load_snmp       (const libconfig::Setting & node_in, snmp_data *const sd);

	void load_switchboard(const libconfig::Setting & node);

	void load_tranceivers(const libconfig::Setting & node, work_queue_t *const w, snmp_data *const sd, stats *const st, ws_global_context_t *const ws);

public:
	configuration(const std::string & file, work_queue_t *const w, snmp_data *const sd, stats *const st);
	virtual ~configuration();

	std::vector<tranceiver *> & get_tranceivers() { return tranceivers; }

	tranceiver    * find_tranceiver(const std::string & id);

	db            * get_db()                { return d;         }

	switchboard   * get_switchboard() const { return sb;        }

	int             get_snmp_port() const   { return snmp_port; }

	ws_global_context_t * get_websockets_context() { return &ws; }

	gps_connector * get_gps() const         { return gps;       }

	std::string   get_logfile() const     { return logfile;   }

	seen          * get_global_repetition_filter() { return global_repetition_filter; }
};
