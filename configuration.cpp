#include <libconfig.h++>

#include "configuration.h"
#include "db-mongodb.h"
#include "error.h"
#include "gps.h"
#include "log.h"
#include "str.h"
#include "tranceiver-aprs-si.h"


configuration::configuration(const std::string & file, work_queue_t *const w, snmp_data *const sd, stats *const st)
{
	try {
		libconfig::Config cfg;

		cfg.readFile(file.c_str());

		const libconfig::Setting & root = cfg.getRoot();

		for(int i=0; i<root.getLength(); i++) {
			const libconfig::Setting & node = root[i];

			std::string node_name = node.getName();

			if (node_name == "filters") {
				load_filters(node);
			}
			else if (node_name == "general") {
				load_general(node);
			}
			else if (node_name == "tranceivers") {
				load_tranceivers(node, w, sd, st, &ws);
			}
			else if (node_name == "bridge-mappings") {
				load_switchboard(node);
			}
			else if (node_name == "snmp") {
				load_snmp(node, sd);
			}
			else {
				error_exit(false, "(line %d): Setting \"%s\" is not known", node.getSourceLine(), node_name.c_str());
			}
		}
	}
        catch(const libconfig::FileIOException &fioex)
        {
                error_exit(false, "I/O error while reading configuration file %s: does it exist? are the access rights correct?", file.c_str());
        }
        catch(const libconfig::ParseException &pex)
        {
                error_exit(false, "Configuration file %s parse error at line %d: %s", pex.getFile(), pex.getLine(), pex.getError());
        }
}

configuration::~configuration()
{
#if HTTP_FOUND == 1
	stop_webserver(webserver);
#endif

#if WEBSOCKETS_FOUND == 1
	stop_websockets();
#endif

	delete sb;

	for(auto t : tranceivers)
		t->stop();

	for(auto t : tranceivers)
		delete t;

	delete d;

	for(auto f : filters)
		delete f.second;

	if (global_repetition_filter) {
		global_repetition_filter->stop();

		delete global_repetition_filter;
	}

	delete gps;
}

void configuration::load_tranceivers(const libconfig::Setting & node_in, work_queue_t *const w, snmp_data *const sd, stats *const st, ws_global_context_t *const ws) {
	for(int i=0; i<node_in.getLength(); i++) {
		const libconfig::Setting & node = node_in[i];

		tranceiver *t = tranceiver::instantiate(node, w, gps, st, i + 1, ws, tranceivers, filters);

		tranceivers.push_back(t);
	}

	// visible in the web-interface as well
	sd->register_oid("1.3.6.1.2.1.2.1.0", snmp_integer::si_integer, tranceivers.size());  // number of network interfaces

	size_t interface_nr = 1;

	for(auto t : tranceivers) {
		// register interface 1
		sd->register_oid(myformat("1.3.6.1.2.1.2.2.1.1.%zu",    interface_nr), snmp_integer::si_integer, interface_nr);
		sd->register_oid(myformat("1.3.6.1.2.1.31.1.1.1.1.%zu", interface_nr), t->get_id());  // name
		sd->register_oid(myformat("1.3.6.1.2.1.2.2.1.2.1.%zu",  interface_nr), t->get_type_name());  // description
		sd->register_oid(myformat("1.3.6.1.2.1.17.1.4.1.%zu",   interface_nr), snmp_integer::si_integer, 1);  // device is up (1)

		t->register_snmp_counters(st, interface_nr);

		interface_nr++;
	}
}

tranceiver * configuration::find_tranceiver(const std::string & id)
{
	for(auto t : tranceivers) {
		if (t->get_id() == id)
			return t;
	}

	return nullptr;
}

void configuration::load_switchboard(const libconfig::Setting & node_in) {
	sb = new switchboard();

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string from   = node.lookup("from"  ).c_str();
		std::string tos    = node.lookup("to"    ).c_str();

		filter *f { nullptr };

		try {
			const std::string filter_name = node.lookup("filter");

			auto it_f = filters.find(filter_name);

			if (it_f == filters.end())
				error_exit(false, "(line %d): Filter \"%s\" is not defined", node.getSourceLine(), filter_name.c_str());

			f = it_f->second;
		}
		catch(libconfig::SettingNotFoundException & e) {
			// perfectly fine
		}

		tranceiver *from_t = find_tranceiver(from);
		if (!from_t)
			error_exit(false, "(line %d): Mapping: \"%s\" is an unknown tranceiver", node.getSourceLine(), from.c_str());

		std::vector<std::string> parts = split(tos, " ");

		for(auto & to : parts) {
			tranceiver *to_t = find_tranceiver(to);
			if (!to_t)
				error_exit(false, "(line %d): Mapping: \"%s\" is an unknown tranceiver", node.getSourceLine(), to.c_str());

			log(LL_DEBUG_VERBOSE, "%s sends to %s", from.c_str(), to.c_str());

			sb->add_mapping(from_t, to_t, f);
		}
        }
}

void configuration::load_snmp(const libconfig::Setting & node_in, snmp_data *const sd)
{
        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "port")
			snmp_port = node_in.lookup(type);
		else
			error_exit(false, "(line %d): SNMP setting \"%s\" is not known", node.getSourceLine(), type.c_str());
        }

	if (snmp_port != -1) {
		sd->register_oid("1.3.6.1.2.1.1.1.0", "lora_aprs_gw");
		sd->register_oid("1.3.6.1.2.1.1.2.0", new snmp_data_type_oid("1.3.6.1.2.1.4.57850.2"));
		sd->register_oid("1.3.6.1.2.1.1.3.0", running_since);
		sd->register_oid("1.3.6.1.2.1.1.4.0", "Folkert van Heusden <mail@vanheusden.com>");
		sd->register_oid("1.3.6.1.2.1.1.5.0", "lora_aprs_gw");
		sd->register_oid("1.3.6.1.2.1.1.6.0", "Earth");
		sd->register_oid("1.3.6.1.2.1.1.7.0", snmp_integer::si_integer, 254 /* everything but the physical layer */);
		sd->register_oid("1.3.6.1.2.1.1.8.0", snmp_integer::si_integer, 0);  // The value of sysUpTime at the time of the most recent change in state or value of any instance of sysORID.
	}
}

void configuration::load_general(const libconfig::Setting & node_in)
{
        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "gps")
			gps = gps_connector::instantiate(node);
		else if (type == "logfile")
			logfile = node_in.lookup(type).c_str();
                else if (type == "repetition-rate-limiting") {
			if (global_repetition_filter)
				error_exit(false, "(line %d): repetition-rate-limiting is already defined", node.getSourceLine());

                        global_repetition_filter = seen::instantiate(node);
                }
		else {
			error_exit(false, "(line %d): General setting \"%s\" is not known", node.getSourceLine(), type.c_str());
		}
        }

	if (!gps)
		error_exit(false, "(line %d): No GPS/location configuration set", node_in.getSourceLine());
}

void configuration::load_filters(const libconfig::Setting & node_in)
{
        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string  name;
		filter      *f { nullptr };

		for(int j=0; j<node.getLength(); j++) {
			const libconfig::Setting & node_def = node[j];

			std::string type = node_def.getName();

			if (type == "name")
				name = node.lookup(type).c_str();
			else if (type == "definition") {
				if (f)
					error_exit(false, "(line %d): Multiple rules definitions found", node_def.getSourceLine());

				f    = filter::instantiate(node_def);
			}
			else {
				error_exit(false, "(line %d): General setting \"%s\" is not known", node_def.getSourceLine(), type.c_str());
			}
		}

		if (!f)
			error_exit(false, "(line %d): No filter defination set for \"%s\"", node_in.getSourceLine(), name.c_str());

		filters.insert({ name, f });
        }
}
