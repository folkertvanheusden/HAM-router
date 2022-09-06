#include "config.h"
#if LIBMONGOCXX_FOUND == 1
#include <bsoncxx/json.hpp>
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

std::vector<std::pair<std::string, uint32_t> > db_mongodb::get_heard_counts()
{
	std::vector<std::pair<std::string, uint32_t> > out;

        mongocxx::database   db              = (*m_c)[database];

        mongocxx::collection work_collection = db[collection];

	mongocxx::pipeline   p { };

	p.group(bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("_id", "$data.from"), bsoncxx::builder::basic::kvp("count", bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("$sum", 1)))));

	p.sort(bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("count", -1)));

	auto cursor = work_collection.aggregate(p, mongocxx::options::aggregate{});

	for(auto doc : cursor) {
		auto value = doc["_id"].get_value();

		if (value.type() == bsoncxx::type::k_null)
			out.push_back({ "-", doc["count"].get_int32().value });
		else
			out.push_back({ value.get_utf8().value.to_string(), doc["count"].get_int32().value });
	}

	return out;
}

std::vector<std::pair<std::string, double> > db_mongodb::get_air_time()
{
	std::vector<std::pair<std::string, double> > out;

        mongocxx::database   db              = (*m_c)[database];

        mongocxx::collection work_collection = db[collection];

	mongocxx::pipeline   p { };

	p.group(bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("_id", "$data.from"), bsoncxx::builder::basic::kvp("air-time", bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("$sum", "$data.air-time")))));

	p.sort(bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("data_from", 1)));

	auto cursor = work_collection.aggregate(p, mongocxx::options::aggregate{});

	for(auto doc : cursor) {
		auto        value    = doc["_id"].get_value();

		std::string name;
		uint32_t    air_time = 0;

		if (value.type() == bsoncxx::type::k_null)
			name = "-";
		else
			name = value.get_utf8().value.to_string();

		if (doc["air-time"].type() == bsoncxx::type::k_int32)
			air_time = doc["air-time"].get_int32().value;

		out.push_back({ name, air_time });
	}

	return out;
}
#endif
