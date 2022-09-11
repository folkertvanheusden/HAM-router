#include <jansson.h>
#include <string>

#include "base64.h"
#include "message.h"
#include "str.h"


message::message(const timeval & tv, const std::string & source, const uint64_t msg_id, const uint8_t *const data, const size_t size) :
	tv      (tv),
	source  (source),
	msg_id  (msg_id),
	b       (data, size)
{
}

message::message(const message & m) :
	tv      (m.get_tv()),
	source  (m.get_source()),
	msg_id  (m.get_msg_id()),
	b       (m.get_buffer())
{
	set_meta(m.get_meta());
}

message::~message()
{
}

std::string message::get_id_short() const
{
	return myformat("%08lx", msg_id);
}

void message::set_meta(const std::map<std::string, db_record_data> & meta_in)
{
	for(auto mrec : meta_in)
		meta[mrec.first] = mrec.second;
}

std::string message_to_json(const message & m)
{
	auto          & meta     = m.get_meta();

	json_t         *json_out = json_object();

	json_object_set_new(json_out, "timestamp", json_integer(m.get_tv().tv_sec));

	json_object_set_new(json_out, "source",    json_string(m.get_source().c_str()));

	json_object_set_new(json_out, "msg-id",    json_string(m.get_id_short().c_str()));

	if (meta.find("air-time") != meta.end())
		json_object_set_new(json_out, "air-time", json_real(meta.at("air-time").d_value));

	if (meta.find("from") != meta.end())
		json_object_set_new(json_out, "from", json_string(meta.at("from").s_value.c_str()));

	if (meta.find("to")   != meta.end())
		json_object_set_new(json_out, "to",   json_string(meta.at("to"  ).s_value.c_str()));

	if (meta.find("latitude")  != meta.end())
		json_object_set_new(json_out, "latitude",  json_real(meta.at("latitude" ).d_value));

	if (meta.find("longitude") != meta.end())
		json_object_set_new(json_out, "longitude", json_real(meta.at("longitude").d_value));

	if (meta.find("protocol")  != meta.end())
		json_object_set_new(json_out, "protocol",  json_string(meta.at("protocol").s_value.c_str()));

	if (meta.find("payload")   != meta.end())
		json_object_set_new(json_out, "payload",   json_string(meta.at("payload").s_value.c_str()));

	if (meta.find("pkt-crc")   != meta.end())
		json_object_set_new(json_out, "pkt-crc",   json_string(meta.at("pkt-crc").s_value.c_str()));

	if (meta.find("rssi")      != meta.end())
		json_object_set_new(json_out, "rssi",      json_string(meta.at("rssi").s_value.c_str()));

	auto content = m.get_content();

	std::string base64_str = base64_encode(content.first, content.second);

	json_object_set_new(json_out, "pkt-base64", json_string(base64_str.c_str()));

	char *json = json_dumps(json_out, 0);

	std::string json_out_str = json;

	free(json);

	json_decref(json_out);

	return json_out_str;
}
