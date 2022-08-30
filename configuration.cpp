#include <libconfig.h++>

#include "configuration.h"
#include "error.h"
#include "log.h"
#include "str.h"
#include "tranceiver-aprs-si.h"


configuration::configuration(const std::string & file, work_queue_t *const w, snmp_data *const sd, stats *const st) :
	sd(sd)
{
	try {
		libconfig::Config cfg;

		cfg.readFile(file.c_str());

		const libconfig::Setting & root = cfg.getRoot();

		for(int i=0; i<root.getLength(); i++) {
			const libconfig::Setting & node = root[i];

			std::string node_name = node.getName();

			if (node_name == "general") {
				// TODO
			}
			else if (node_name == "tranceivers") {
				load_tranceivers(node, w, st);
			}
			else if (node_name == "connections") {
				load_switchboard(node);
			}
			else if (node_name == "snmp") {
				load_snmp(node);
			}
			else {
				error_exit(false, "Setting \"%s\" is now known", node_name.c_str());
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
	delete sb;

	for(auto t : tranceivers)
		delete t;
}

void configuration::load_tranceivers(const libconfig::Setting & node_in, work_queue_t *const w, stats *const st) {
	for(int i=0; i<node_in.getLength(); i++) {
		const libconfig::Setting & node = node_in[i];

		tranceiver *t = tranceiver::instantiate(node, w);

		tranceivers.push_back(t);
	}

	if (snmp_port != -1) {
		sd->register_oid("1.3.6.1.2.1.2.1.0", snmp_integer::si_integer, tranceivers.size());  // number of network interfaces

		int interface_nr = 1;

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

		std::string from = node.lookup("from").c_str();
		std::string to   = node.lookup("to"  ).c_str();

		tranceiver *from_t = find_tranceiver(from);
		if (!from_t)
			error_exit(false, "Mapping: \"%s\" is an unknown tranceiver", from.c_str());

		tranceiver *to_t   = find_tranceiver(to);
		if (!to_t)
			error_exit(false, "Mapping: \"%s\" is an unknown tranceiver", to.c_str());

		sb->add_mapping(from_t, to_t);

		log(LL_DEBUG_VERBOSE, "%s sends to %s", from.c_str(), to.c_str());
        }
}

void configuration::load_snmp(const libconfig::Setting & node_in)
{
        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "port")
			snmp_port = node_in.lookup(type);
		else
			error_exit(false, "SNMP setting \"%s\" is now known", type.c_str());
        }

	if (snmp_port != -1) {
		sd->register_oid("1.3.6.1.2.1.1.1.0", "lora_aprs_gw");
		sd->register_oid("1.3.6.1.2.1.1.2.0", new snmp_data_type_oid("1.3.6.1.2.1.4.57850.2"));
		sd->register_oid("1.3.6.1.2.1.1.3.0", &running_since);
		sd->register_oid("1.3.6.1.2.1.1.4.0", "Folkert van Heusden <mail@vanheusden.com>");
		sd->register_oid("1.3.6.1.2.1.1.5.0", "lora_aprs_gw");
		sd->register_oid("1.3.6.1.2.1.1.6.0", "Earth");
		sd->register_oid("1.3.6.1.2.1.1.7.0", snmp_integer::si_integer, 254 /* everything but the physical layer */);
		sd->register_oid("1.3.6.1.2.1.1.8.0", snmp_integer::si_integer, 0);  // The value of sysUpTime at the time of the most recent change in state or value of any instance of sysORID.
	}
}
