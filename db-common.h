#pragma once

#include <map>
#include <string>
#include <sys/time.h>

#include "buffer.h"


typedef enum {
	dt_octetArray,
	dt_unsigned8,
	dt_unsigned16,
	dt_unsigned32,
	dt_signed8,
	dt_signed16,
	dt_signed32,
	dt_signed64,
	dt_float64,
	dt_boolean,
	dt_string,
} data_type_t;

struct db_record_data
{
	buffer      b;

	std::string s_value;
	uint64_t    i_value;
	double      d_value;

	data_type_t dt;
};

struct db_record
{
	const timeval tv;

	// key, value
	std::map<std::string, db_record_data> kvs;

	db_record(const timeval & tv) : tv(tv) {
	}
};

void db_record_insert(db_record *const r, const std::string & name, const buffer & b);
void db_record_insert(db_record *const r, const std::string & name, const std::string & str);
void db_record_insert(db_record *const r, const std::string & name, const int64_t v);
void db_record_insert(db_record *const r, const std::string & name, const double v);
