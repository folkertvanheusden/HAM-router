#include <string>
#include "db-common.h"


void db_record_insert(db_record *const r, const std::string & name, const buffer & b)
{
	db_record_data rec(b);
	rec.dt      = dt_octetArray;

	r->kvs.insert({ name, rec });
}

void db_record_insert(db_record *const r, const std::string & name, const std::string & str)
{
	db_record_data rec;
	rec.s_value = str;
	rec.dt      = dt_string;

	r->kvs.insert({ name, rec });
}

void db_record_insert(db_record *const r, const std::string & name, const int64_t v)
{
	db_record_data rec;
	rec.i_value = v;
	rec.dt      = dt_signed64;

	r->kvs.insert({ name, rec });
}

void db_record_insert(db_record *const r, const std::string & name, const double v)
{
	db_record_data rec;
	rec.d_value = v;
	rec.dt      = dt_float64;

	r->kvs.insert({ name, rec });
}
