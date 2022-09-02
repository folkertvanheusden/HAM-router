#include <string>
#include "db-common.h"


db_record_data db_record_gen(const buffer & b)
{
	db_record_data rec(b);
	rec.dt      = dt_octetArray;

	return rec;
}

db_record_data db_record_gen(const std::string & str)
{
	db_record_data rec;
	rec.s_value = str;
	rec.dt      = dt_string;

	return rec;
}

db_record_data db_record_gen(const int64_t v)
{
	db_record_data rec;
	rec.i_value = v;
	rec.dt      = dt_signed64;

	return rec;
}

db_record_data db_record_gen(const double v)
{
	db_record_data rec;
	rec.d_value = v;
	rec.dt      = dt_float64;

	return rec;
}

void db_record_insert(db_record *const r, const std::string & name, const db_record_data & data)
{
	r->kvs.insert({ name, data });
}
