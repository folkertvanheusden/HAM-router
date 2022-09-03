#pragma once

#include <map>
#include <stdlib.h>
#include <string>
#include <sys/time.h>

#include "buffer.h"
#include "db-common.h"


class message {
private:
	const timeval     tv;

	const std::string source;

	const uint64_t    msg_id;

	const bool        from_rf;   // did it come from electromagnetic waves?
	const int         air_time;  // in milliseconds

	const buffer      b;

	std::map<std::string, db_record_data> meta;

public:
	message(const timeval & tv, const std::string & source, const uint64_t msg_id, const bool from_rf, const int air_time, const uint8_t *const data, const size_t size);

	message(const message & m);

	virtual ~message();

	timeval        get_tv()         const { return tv;       }

	std::string    get_source()     const { return source;   }

	uint64_t       get_msg_id()     const { return msg_id;   }

	bool           get_is_from_rf() const { return from_rf;  }

	int            get_air_time()   const { return air_time; }

	const buffer & get_buffer()     const { return b;               }

	auto           get_content()    const { return b.get_content(); }

	std::string    get_id_short()   const;

	void           set_meta(const std::map<std::string, db_record_data> & meta);

	auto         & get_meta()       const { return meta;    }
};
