#include <string>

#include "message.h"
#include "str.h"


message::message(const timeval & tv, const std::string & source, const uint64_t msg_id, const bool from_rf, const int air_time, const uint8_t *const data, const size_t size) :
	tv      (tv),
	source  (source),
	msg_id  (msg_id),
	from_rf (from_rf),
	air_time(air_time),
	b       (data, size)
{
}

message::message(const message & m) :
	tv      (m.get_tv()),
	source  (m.get_source()),
	msg_id  (m.get_msg_id()),
	from_rf (m.get_is_from_rf()),
	air_time(m.get_air_time()),
	b       (m.get_buffer()),
	meta    (m.get_meta())
{
}

message::~message()
{
}

std::string message::get_id_short() const
{
	return myformat("%08x", msg_id);
}

void message::set_meta(const std::map<std::string, db_record_data> & meta)
{
	this->meta = meta;
}
