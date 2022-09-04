#include <string>

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
	return myformat("%08x", msg_id);
}

void message::set_meta(const std::map<std::string, db_record_data> & meta_in)
{
	for(auto mrec : meta_in)
		meta[mrec.first] = mrec.second;
}
