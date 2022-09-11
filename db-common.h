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
	dt_none
} data_type_t;

struct db_record_data
{
	data_type_t dt;

	buffer      b;

	std::string s_value;
	uint64_t    i_value { 0 };
	double      d_value { 0 };

	db_record_data() {
		dt = dt_none;
	}

	db_record_data(const buffer & b) :
		dt(dt_octetArray),
		b(b) {
	}

	db_record_data(const std::string & s) :
		dt(dt_string),
		s_value(s) {
	}

	db_record_data & operator=(const db_record_data & in) {
		dt      = in.dt;
		b       = in.b;
		s_value = in.s_value;
		i_value = in.i_value;
		d_value = in.d_value;

		return *this;
	}

	db_record_data(const db_record_data & other) :
		dt(other.dt),
		b(other.b),
		s_value(other.s_value),
		i_value(other.i_value),
		d_value(other.d_value) {
	}
};

struct db_record
{
	const timeval tv;

	// key, value
	std::map<std::string, db_record_data> kvs;

	db_record(const timeval & tv) : tv(tv) {
	}
};

db_record_data db_record_gen(const buffer & b);
db_record_data db_record_gen(const std::string & str);
db_record_data db_record_gen(const int64_t v);
db_record_data db_record_gen(const double v);

void db_record_insert(db_record *const r, const std::string & name, const db_record_data & data);
