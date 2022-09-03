#include <libconfig.h++>
#include <regex.h>
#include <string>
#include <vector>

#include "db-common.h"
#include "message.h"


typedef enum { FC_equal, FC_substr, FC_less_than, FC_bigger_than, FC_regex, FC_invalid } filter_compare_t;

class filter_rule
{
private:
	const std::string      field_name;
	const db_record_data   value;
	const filter_compare_t how;
	const bool             not_;
	const bool             ignore_if_missing;
	const bool             ignore_data_type_mismatch;
	const bool             ignore_action_not_applicable_to_data_type;
	regex_t                re { 0 };

public:
	filter_rule(const std::string & field_name, const db_record_data & value, const filter_compare_t fc, const bool not_, const bool ignore_if_missing, const bool ignore_data_type_mismatch, const bool ignore_action_not_applicable_to_data_type);
	virtual ~filter_rule();

	bool check(const message & m);

	static filter_rule *instantiate(const libconfig::Setting & node);
};

typedef enum { FR_all, FR_one, FR_none, FR_invalid } filter_rule_matching_t;

class filter
{
private:
	const std::vector<filter_rule *> rules;
	const filter_rule_matching_t     how;

public:
	// transfers ownership of rules
	filter(const std::vector<filter_rule *> & rules, const filter_rule_matching_t how);
	virtual ~filter();

	bool check(const message & m);

	static filter *instantiate(const libconfig::Setting & node_in);
};
