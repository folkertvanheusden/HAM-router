#pragma once

#include <map>
#include <stdlib.h>
#include <string>
#include <sys/time.h>

#include "buffer.h"
#include "db-common.h"


class tranceiver;

class message {
private:
	const timeval     tv     { 0, 0    };

	const tranceiver *source { nullptr };

	const uint64_t    msg_id { 0       };

	const buffer      b;

	std::map<std::string, db_record_data> meta;

public:
	message(const timeval & tv, const tranceiver *const source, const uint64_t msg_id, const uint8_t *const data, const size_t size);

	message(const message & m);

	virtual ~message();

	timeval        get_tv()         const { return tv;       }

	const tranceiver * get_source() const { return source;   }

	uint64_t       get_msg_id()     const { return msg_id;   }

	const buffer & get_buffer()     const { return b;               }

	auto           get_content()    const { return b.get_content(); }

	std::string    get_id_short()   const;

	void           set_meta(const std::map<std::string, db_record_data> & meta);

	auto         & get_meta()       const { return meta;    }
};

std::string message_to_json(const message & m);

void dump_meta(const std::map<std::string, db_record_data> & meta);
