#include "config.h"
#if LIBMONGOCXX_FOUND == 1
#include <chrono>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/instance.hpp>

#include "db-mongodb.h"
#include "dissect-packet.h"
#include "gps.h"
#include "log.h"
#include "time.h"

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

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

	doc.append(kvp("receive-time", bsoncxx::types::b_date(to_time_point(dr.tv))));

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

					sub_doc.append(kvp(key_name, binary_data));
				}
				break;

			case dt_unsigned8:
				sub_doc.append(kvp(key_name, uint8_t(element_data.i_value)));
				break;

			case dt_unsigned16:
				sub_doc.append(kvp(key_name, uint16_t(element_data.i_value)));
				break;

			case dt_unsigned32:
				sub_doc.append(kvp(key_name, int64_t(uint32_t(element_data.i_value))));
				break;

			case dt_signed8:
				sub_doc.append(kvp(key_name, int8_t(element_data.i_value)));
				break;

			case dt_signed16:
				sub_doc.append(kvp(key_name, int16_t(element_data.i_value)));
				break;

			case dt_signed32:
				sub_doc.append(kvp(key_name, int64_t(element_data.i_value)));
				break;

			case dt_signed64:
				sub_doc.append(kvp(key_name, int64_t(element_data.i_value)));
				break;

			case dt_float64:
				sub_doc.append(kvp(key_name, double(element_data.d_value)));
				break;

			case dt_boolean:
				sub_doc.append(kvp(key_name, !!element_data.i_value));
				break;

			case dt_string:
				sub_doc.append(kvp(key_name, element_data.s_value));
				break;

			default:
				log(LL_WARNING, "db_mongodb::insert: data type %d not supported for MongoDB target", element_data.dt);

				return false;
		}
	}

	doc.append(kvp("data", sub_doc));

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

std::vector<std::pair<std::string, uint32_t> > db_mongodb::get_simple_groupby(const std::string & field)
{
	std::vector<std::pair<std::string, uint32_t> > out;

        mongocxx::database   db              = (*m_c)[database];

        mongocxx::collection work_collection = db[collection];

	mongocxx::pipeline   p { };

	p.group(make_document(kvp("_id", "$data." + field), kvp("count", make_document(kvp("$sum", 1)))));

	p.sort(make_document(kvp("count", -1)));

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

std::vector<std::pair<std::string, uint32_t> > db_mongodb::get_heard_counts()
{
	return get_simple_groupby("from");
}

std::vector<std::pair<std::string, uint32_t> > db_mongodb::get_protocol_counts()
{
	return get_simple_groupby("protocol");
}

std::vector<std::pair<std::string, uint32_t> > db_mongodb::get_to_counts()
{
	return get_simple_groupby("to");
}

std::vector<std::pair<std::pair<std::string, std::string>, double> > db_mongodb::get_air_time()
{
	std::vector<std::pair<std::pair<std::string, std::string>, double> > out;

        mongocxx::database   db              = (*m_c)[database];

        mongocxx::collection work_collection = db[collection];

	mongocxx::pipeline   p { };

	p.group(make_document(kvp("_id", make_document(kvp("id", "$data.from"), kvp("date", make_document(kvp("$dateToString", make_document(kvp("format", "%Y-%m-%d"), kvp("date", "$receive-time"))))))), kvp("air-time", make_document(kvp("$sum", "$data.air-time")))));

	p.sort(make_document(kvp("_id", 1)));

	auto cursor = work_collection.aggregate(p, mongocxx::options::aggregate{});

	for(auto doc : cursor) {
		double      air_time  = 0;

		auto        name_doc  = doc["_id"];
		auto        name_id   = name_doc["id"];
		auto        name_date = name_doc["date"];

		std::string name_id_str;
		std::string name_date_str;

		if (name_id && name_id.type() != bsoncxx::type::k_null)
			name_id_str = name_id.get_utf8().value.to_string();

		if (name_date && name_date.type() != bsoncxx::type::k_null)
			name_date_str = name_date.get_utf8().value.to_string();

		if (doc["air-time"].type() == bsoncxx::type::k_double)
			air_time = doc["air-time"].get_double().value / 1000.;

		out.push_back({ { name_id_str, name_date_str }, air_time });
	}

	return out;
}

std::map<std::string, uint32_t> db_mongodb::get_misc_counts()
{
	std::map<std::string, uint32_t > out;

        mongocxx::database   db              = (*m_c)[database];

        mongocxx::collection work_collection = db[collection];

	{
		mongocxx::pipeline p { };

		p.group(make_document(kvp("_id", "$data.payload")));

		auto cursor = work_collection.aggregate(p, mongocxx::options::aggregate{});

		out.insert({ "number of unique payloads", cursor.begin() == cursor.end() ? 0 : std::distance(cursor.begin(), cursor.end()) });
	}

	{
		mongocxx::pipeline p { };

		p.group(make_document(kvp("_id", "$data.raw-data")));

		auto cursor = work_collection.aggregate(p, mongocxx::options::aggregate{});

		out.insert({ "number of unique packets", cursor.begin() == cursor.end() ? 0 : std::distance(cursor.begin(), cursor.end()) });
	}

	{
		auto val = db.run_command(make_document(kvp("count", collection)));

		out.insert({ "total number of records", val.view()["n"].get_int32().value });
	}

	return out;
}

std::vector<message> db_mongodb::get_history(const std::string & callsign, const int n)
{
	std::vector<message> out;

        mongocxx::database   db              = (*m_c)[database];

        mongocxx::collection work_collection = db[collection];

	mongocxx::options::find opts;
	opts.limit(n);
	opts.sort(make_document(kvp("receive-time", -1)));

	auto cursor = work_collection.find(make_document(kvp("data.from", callsign)), opts);

	for(auto doc : cursor) {
		auto data = doc["data"];

		if (data) {
			timeval     tv       = to_timeval(doc["receive-time"].get_date().value);

			std::string source   = data["source"] ? data["source"].get_utf8().value.to_string() : "?";

			uint64_t    msg_id   = data["msg-id"] ? data["msg-id"].get_int64().value : 0;

			double      air_time = data["air-time"] ? data["air-time"].get_double().value : 0;

			auto        pkt      = data["raw-data"];

			const uint8_t *bin_p    = pkt ? pkt.get_binary().bytes : reinterpret_cast<const uint8_t *>("");
			int            bin_size = pkt ? pkt.get_binary().size : 1;

			message m(tv, source, msg_id, bin_p, bin_size);

			// TODO: move this into a function of some sort, see also tranceiver.cpp
			auto        meta    = dissect_packet(bin_p, bin_size);

			if (meta.has_value()) {
				meta.value().insert({ "air-time", db_record_gen(double(air_time)) });

				m.set_meta(meta.value());
			}

			out.push_back(m);
		}
	}

	return out;
}
#endif
