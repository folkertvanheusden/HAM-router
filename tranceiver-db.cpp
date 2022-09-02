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

	db_record_insert(&record, "raw-data", m.get_buffer());

	db_record_insert(&record, "source", m.get_source());

	db_record_insert(&record, "msg-id", int64_t(m.get_msg_id()));

	if (m.get_is_from_rf())
		db_record_insert(&record, "air-time", double(m.get_air_time()));

	d->insert(record);
}

transmit_error_t tranceiver_db::put_message_low(const message & m)
{
	insert_into_database(d, m);

	return TE_ok;
}

tranceiver_db::tranceiver_db(const std::string & id, seen *const s, work_queue_t *const w, db *const d) :
	tranceiver(id, s, w),
	d(d)
{
	log(LL_INFO, "Instantiated MongoDB (%s)", id.c_str());
}

tranceiver_db::~tranceiver_db()
{
	delete d;
}

void tranceiver_db::operator()()
{
}

tranceiver *tranceiver_db::instantiate(const libconfig::Setting & node_in, work_queue_t *const w)
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
		else if (type == "incoming-rate-limiting") {
			assert(s == nullptr);
			s = seen::instantiate(node);
		}
		else if (type == "uri")
			db_uri = node_in.lookup(type).c_str();
		else if (type == "database")
			db_database = node_in.lookup(type).c_str();
		else if (type == "collection")
			db_collection = node_in.lookup(type).c_str();
		else if (type != "type") {
			error_exit(false, "Database setting \"%s\" is now known", type.c_str());
		}
        }

	if (db_uri.empty() == false) {
		if (db_database.empty() == true || db_collection.empty() == true)
			error_exit(false, "MongoDB is missing settings");

		d = new db_mongodb(db_uri, db_database, db_collection);

		d->init_database();
        }

	return new tranceiver_db(id, s, w, d);
#else
	log(LL_ERROR, "No MongoDB support compiled in!");

	return nullptr;
#endif
}
