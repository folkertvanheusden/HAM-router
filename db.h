#pragma once
#include <map>
#include <stdint.h>
#include <string>
#include <time.h>

#include "db-common.h"


class db
{
public:
	db();
	virtual ~db();

	virtual void init_database() = 0;

	virtual bool insert(const db_record & dr) = 0;
};
