#pragma once

#include <libconfig.h++>
#include <regex.h>
#include <string>
#include <vector>

#include "db-common.h"
#include "message.h"


typedef struct
{
	bool        ignore_if_field_is_missing;
	std::string pattern;
} filter_t;

bool execute_filter(const std::string & pattern, const bool ignore_if_field_is_missing, const message & m);
