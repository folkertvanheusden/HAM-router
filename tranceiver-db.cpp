#include "config.h"
#include <assert.h>
#include <errno.h>
#include <optional>
#include <string>
#include <string.h>
#include <unistd.h>

#include "db-mongodb.h"
#include "error.h"
#include "log.h"
#include "net.h"
#include "random.h"
#include "str.h"
#include "tranceiver-db.h"
#include "utils.h"


void insert_into_database(db *const d, const message & m)
{
	db_record record(m.get_tv());

	db_record_insert(&record, "raw-data", db_record_gen(m.get_buffer()));

	db_record_insert(&record, "source", db_record_gen(m.get_source()));

	db_record_insert(&record, "msg-id", db_record_gen(int64_t(m.get_msg_id())));

	for(auto r : m.get_meta())
		db_record_insert(&record, r.first, r.second);

	d->insert(record);
}

transmit_error_t tranceiver_db::put_message_low(const message & m)
{
	insert_into_database(d, m);

	return TE_ok;
}

tranceiver_db::tranceiver_db(const std::string & id, seen *const s, work_queue_t *const w, gps_connector *const gps, db *const d) :
	tranceiver(id, s, w, gps),
	d(d)
{
	log(LL_INFO, "Instantiated MongoDB");
}

tranceiver_db::~tranceiver_db()
{
	delete d;
}

void tranceiver_db::operator()()
{
}

tranceiver *tranceiver_db::instantiate(const libconfig::Setting & node_in, work_queue_t *const w, gps_connector *const gps)
{
#if LIBMONGOCXX_FOUND == 1
	std::string  id;
	seen        *s { nullptr };
	std::string  db_uri;
	std::string  db_database;
	std::string  db_collection;
	db          *d { nullptr };

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "repetition-rate-limiting") {
			if (s)
				error_exit(false, "db(line %d): repetition-rate-limiting already set", node.getSourceLine());

			s = seen::instantiate(node);
		}
		else if (type == "uri")
			db_uri = node_in.lookup(type).c_str();
		else if (type == "database")
			db_database = node_in.lookup(type).c_str();
		else if (type == "collection")
			db_collection = node_in.lookup(type).c_str();
		else if (type != "type") {
			error_exit(false, "(line %d): Database setting \"%s\" is not known", node.getSourceLine(), type.c_str());
		}
        }

	if (db_uri.empty() == false) {
		if (db_database.empty() == true || db_collection.empty() == true)
			error_exit(false, "(line %d): MongoDB is missing settings", node_in.getSourceLine());

		d = new db_mongodb(db_uri, db_database, db_collection);

		d->init_database();
        }

	return new tranceiver_db(id, s, w, gps, d);
#else
	llog(LL_ERROR, node_in, "No MongoDB support compiled in!");

	error_exit(false, "No MongoDB support compiled in!");

	return nullptr;
#endif
}
