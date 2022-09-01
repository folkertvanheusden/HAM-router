#include "config.h"
#ifdef HAVE_LIBMONGOCXX
#include <bsoncxx/builder/stream/document.hpp>
#include <chrono>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

#include "db-mongodb.h"
#include "log.h"
#include "time.h"


mongocxx::instance instance { };

db_mongodb::db_mongodb(const std::string & uri, const std::string & database, const std::string & collection) :
	database(database), collection(collection)
{
	mongocxx::uri m_uri(uri);

	m_c = new mongocxx::client(m_uri);
}

db_mongodb::~db_mongodb()
{
	delete m_c;
}

void db_mongodb::init_database()
{
}

bool db_mongodb::insert(const db_record & dr)
{
	mongocxx::database                db              = (*m_c)[database];

	mongocxx::collection              work_collection = db[collection];

	bsoncxx::builder::basic::document doc;

	doc.append(bsoncxx::builder::basic::kvp("receive-time", bsoncxx::types::b_date(to_time_point(dr.tv))));

	bsoncxx::builder::basic::document sub_doc;

	for(auto & element : dr.kvs) {
		db_record_data element_data = element.second;

		std::string    key_name     = element.first;

		switch(element_data.dt) {
			case dt_octetArray: {
					const int      len   = element_data.b.get_n_bytes_left();
					const uint8_t *bytes = element_data.b.get_bytes(len);

					bsoncxx::types::b_binary binary_data;
					binary_data.size  = len;
					binary_data.bytes = bytes;

					sub_doc.append(bsoncxx::builder::basic::kvp(key_name, binary_data));
				}
				break;

			case dt_unsigned8:
				sub_doc.append(bsoncxx::builder::basic::kvp(key_name, uint8_t(element_data.i_value)));
				break;

			case dt_unsigned16:
				sub_doc.append(bsoncxx::builder::basic::kvp(key_name, uint16_t(element_data.i_value)));
				break;

			case dt_unsigned32:
				sub_doc.append(bsoncxx::builder::basic::kvp(key_name, int64_t(uint32_t(element_data.i_value))));
				break;

			case dt_signed8:
				sub_doc.append(bsoncxx::builder::basic::kvp(key_name, int8_t(element_data.i_value)));
				break;

			case dt_signed16:
				sub_doc.append(bsoncxx::builder::basic::kvp(key_name, int16_t(element_data.i_value)));
				break;

			case dt_signed32:
				sub_doc.append(bsoncxx::builder::basic::kvp(key_name, int64_t(element_data.i_value)));
				break;

			case dt_signed64:
				sub_doc.append(bsoncxx::builder::basic::kvp(key_name, int64_t(element_data.i_value)));
				break;

			case dt_float64:
				sub_doc.append(bsoncxx::builder::basic::kvp(key_name, double(element_data.d_value)));
				break;

			case dt_boolean:
				sub_doc.append(bsoncxx::builder::basic::kvp(key_name, !!element_data.i_value));
				break;

			case dt_string:
				sub_doc.append(bsoncxx::builder::basic::kvp(key_name, element_data.s_value));
				break;

			default:
				log(LL_WARNING, "db_mongodb::insert: data type %d not supported for MongoDB target", element_data.dt);

				return false;
		}
	}

	doc.append(bsoncxx::builder::basic::kvp("data", sub_doc));

	bsoncxx::stdx::optional<mongocxx::result::insert_one> result = work_collection.insert_one(doc.view());

	if (!result) {
		log(LL_WARNING, "db_mongodb::insert: no result for document insert returned");

		return false;
	}

	int32_t inserted_count = result->result().inserted_count();

	if (inserted_count != 1) {
		log(LL_WARNING, "db_mongodb::insert: unexpected (%d) inserted count (expected 1)", inserted_count);

		return false;
	}

	return true;
}
#endif
