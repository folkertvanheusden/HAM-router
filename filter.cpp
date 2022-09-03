#include "error.h"
#include "filter.h"
#include "log.h"


filter_rule::filter_rule(const std::string & field_name, const db_record_data & value, const filter_compare_t how, const bool not_, const bool ignore_if_missing, const bool ignore_data_type_mismatch, const bool ignore_action_not_applicable_to_data_type) :
	field_name               (field_name),
	value                    (value),
	how                      (how),
	not_                     (not_),
	ignore_if_missing        (ignore_if_missing),
	ignore_data_type_mismatch(ignore_data_type_mismatch),
	ignore_action_not_applicable_to_data_type(ignore_action_not_applicable_to_data_type)
{
	if (value.dt == dt_string && how == FC_regex) {
		if (regcomp(&re, value.s_value.c_str(), REG_EXTENDED))
			error_exit(false, "Failed to compile regular expression \"%s\"", value.s_value.c_str());
	}
}

filter_rule::~filter_rule()
{
	if (how == FC_regex)
		regfree(&re);
}

bool filter_rule::check(const message & m)
{
	const auto & meta = m.get_meta();

	const auto it     = meta.find(field_name);

	if (it == meta.end())
		return ignore_if_missing;

	if (it->second.dt != value.dt)
		return ignore_data_type_mismatch;

	if (value.dt == dt_string) {
		if (how == FC_equal)
			return not_ ? it->second.s_value != value.s_value : it->second.s_value == value.s_value;

		if (how == FC_substr)
			return not_ ? it->second.s_value.find(value.s_value) == std::string::npos : it->second.s_value.find(value.s_value) != std::string::npos;

		if (how == FC_regex) {
			int rc = regexec(&re, it->second.s_value.c_str(), 0, nullptr, 0);

			if (rc == REG_NOMATCH)
				return not_ ? true : false;

			if (rc == 0)
				return not_ ? false : true;

			char errbuf[128] { 0 };
			regerror(rc, &re, errbuf, sizeof errbuf);

			log(LL_ERROR, "regexec failed on \"%s\" with \"%s\": %s", it->second.s_value.c_str(), value.s_value.c_str(), errbuf);

			return false;
		}

		return ignore_action_not_applicable_to_data_type;
	}
	else {
		error_exit(false, "Internal error: unknown data type");
	}

	log(LL_ERROR, "This should not be reached.");

	return false;
}

filter_rule *filter_rule::instantiate(const libconfig::Setting & node_in)
{
	std::string      field_name;
	std::string      value_type;
	db_record_data   value;
	bool             not_                      { false   };
	bool             ignore_if_missing         { false   };
	bool             ignore_data_type_mismatch { false   };
	bool             ignore_action_not_applicable_to_data_type { false };
	filter_compare_t fc                        { FC_invalid };

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "field-name")
			field_name = node_in.lookup(type).c_str();
		else if (type == "value-type")
			value_type = node_in.lookup(type).c_str();
		else if (type == "value-data") {
			if (value_type.empty())
				error_exit(false, "Filter rule: need to set a \"value-type\" first");

			if (value_type == "string")
				value.dt = dt_string, value.s_value = node_in.lookup(type).c_str();
			else if (value_type == "integer")
				value.dt = dt_signed64, value.i_value = node_in.lookup(type);
			else if (value_type == "float")
				value.dt = dt_float64, value.d_value = node_in.lookup(type);
			else
				error_exit(false, "Filter rule: unknown value-type (%s)", value_type.c_str());
		}
		else if (type == "not")
			not_ = node_in.lookup(type);
		else if (type == "ignore-if-field-is-missing")
			ignore_if_missing = node_in.lookup(type);
		else if (type == "ignore-data-type-mismatch")
			ignore_data_type_mismatch = node_in.lookup(type);
		else if (type == "ignore-action-not-applicable-to-data-type")
			ignore_action_not_applicable_to_data_type = node_in.lookup(type);
		else if (type == "how-to-compare") {
			std::string how_to = node_in.lookup(type).c_str();

			if (how_to == "equal")
				fc = FC_equal;
			else if (how_to == "substr")
				fc = FC_substr;
			else if (how_to == "less-than")
				fc = FC_less_than;
			else if (how_to == "bigger-than")
				fc = FC_bigger_than;
			else if (how_to == "regex")
				fc = FC_regex;
			else {
				error_exit(false, "How-to-compare is invalid (%s)", how_to.c_str());
			}
		}
		else {
			error_exit(false, "Filter_rule: \"%s\" is not known", type.c_str());
		}
        }

	if (value.dt == dt_none)
		error_exit(false, "No value-type set");

	if (fc == FC_invalid)
		error_exit(false, "How to compare not set");

	return new filter_rule(field_name, value_type, fc, not_, ignore_if_missing, ignore_data_type_mismatch, ignore_action_not_applicable_to_data_type);

}

filter::filter(const std::vector<filter_rule *> & rules, const filter_rule_matching_t how) :
	rules(rules),
	how  (how)
{
}

filter::~filter()
{
	for(auto rule : rules)
		delete rule;
}

bool filter::check(const message & m)
{
	for(auto rule : rules) {
		bool rc = rule->check(m);

		if (rc == false && how == FR_all)
			return false;
		
		if (rc == true) {
			if (how == FR_one)
				return true;

			if (how == FR_none)
				return false;
		}
	}

	return true;
}

filter *filter::instantiate(const libconfig::Setting & node_in)
{
	std::vector<filter_rule *> rules;
	filter_rule_matching_t     how   { FR_invalid };

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "rules") {
			for(int j=0; j<node.getLength(); j++) {
				const libconfig::Setting & rule_node = node[j];

				rules.push_back(filter_rule::instantiate(rule_node));
			}
		}
		else if (type == "how-to-compare") {
			std::string how_to = node_in.lookup(type).c_str();

			if (how_to == "all")
				how = FR_all;
			else if (how_to == "one")
				how = FR_one;
			else if (how_to == "none")
				how = FR_none;
			else {
				error_exit(false, "How-to-compare is invalid (%s)", how_to.c_str());
			}
		}
		else {
			error_exit(false, "Filter: \"%s\" is not known", type.c_str());
		}
        }

	if (rules.empty())
		error_exit(false, "No rules defined");

	if (how == FR_invalid)
		error_exit(false, "How to compare not set");

	return new filter(rules, how);
}
