#pragma once

#include <map>
#include <stdint.h>
#include <string>
#include <time.h>
#include <vector>

#include "db-common.h"
#include "message.h"


class configuration;

class db
{
public:
	db();
	virtual ~db();

	virtual void init_database() = 0;

	virtual bool insert(const db_record & dr) = 0;

	virtual std::vector<std::pair<std::string, uint32_t> > get_heard_counts() = 0;

	virtual std::vector<std::pair<std::string, uint32_t> > get_protocol_counts() = 0;

	virtual std::vector<std::pair<std::string, uint32_t> > get_to_counts() = 0;

	virtual std::vector<std::pair<std::pair<std::string, std::string>, std::pair<double, int> > > get_air_time() = 0;

	virtual std::map<std::string, uint32_t> get_misc_counts() = 0;

	virtual std::vector<message> get_history(const std::string & callsign, const std::string & date, const bool ignore_callsign, configuration *const cfg) = 0;

	virtual std::vector<std::tuple<int, int, uint32_t> > get_heatmap() = 0;
};
