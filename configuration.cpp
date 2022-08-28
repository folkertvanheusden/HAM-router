#include <libconfig.h++>

#include "configuration.h"
#include "error.h"
#include "tranceiver-aprs-si.h"


configuration::configuration(const std::string & file, work_queue_t *const w)
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
				load_tranceivers(node, w);
			}
			else if (node_name == "connections") {
				load_switchboard(node);
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

void configuration::load_tranceivers(const libconfig::Setting & node_in, work_queue_t *const w) {
	for(int i=0; i<node_in.getLength(); i++) {
		const libconfig::Setting & node = node_in[i];

		tranceiver *t = tranceiver::instantiate(node, w);

		tranceivers.push_back(t);
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
		tranceiver *to_t   = find_tranceiver(to);

		sb->add_mapping(from_t, to_t);
        }
}
