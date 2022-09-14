#include <ctype.h>
#include <optional>

#include "error.h"
#include "filter.h"
#include "log.h"
#include "str.h"


std::optional<std::string> retrieve_subfilter(const std::string & in)
{
	bool string = false;
	int  level  = 1;

	for(size_t i=0; i<in.size(); i++) {
		if (in[i] == ')' && string == false) {
			level--;

			if (level == 0)
				return in.substr(0, i);
		}
		else if (in[i] == '\"')
			string = !string;

		else if (in[i] == '(' && string == false) {
			level++;
		}
	}

	return { };  // end of subfilter not found
}

bool execute_filter(const std::string & filter, const bool ignore_if_field_is_missing, const message & m)
{
	std::size_t position  = 0;
	std::string operation;
	bool        rc        = false;

	while(position < filter.size()) {
		if (filter[position] == ' ') {
			position++;

			continue;
		}

		if (filter[position] == '(') {
			auto temp = retrieve_subfilter(filter.substr(position + 1));

			if (temp.has_value() == false)
				error_exit(false, "End of sub-filter not found");

			bool cur_rc = execute_filter(temp.value(), ignore_if_field_is_missing, m);

			if (operation.empty())
				rc = cur_rc;
			else if (operation == "&&")
				rc = rc && cur_rc;
			else if (operation == "||")
				rc = rc || cur_rc;
			else
				error_exit(false, "Unknown operation \"%s\"", operation.c_str());

			operation.clear();

			position += temp.value().size() + 2;
		}
		else if (filter[position] == '&' || filter[position] == '|') {
			std::size_t token_end = filter.find_first_of(" ", position);

			operation = filter.substr(position, token_end - position);

			position = token_end;
		}
		else {
			std::size_t equal_sign  = filter.find_first_of("=",  position);
			std::size_t nequal_sign = filter.find_first_of("!=", position);

			std::size_t splitter    = (equal_sign != std::string::npos && equal_sign < nequal_sign) || nequal_sign == std::string::npos ? equal_sign : nequal_sign;
			bool        equals      = splitter == equal_sign;
			int         compare_len = equals ? 1 : 2;

			std::string left        = trim(filter.substr(position, splitter - position));

			std::size_t right_end   = filter.find_first_of(" ", splitter + compare_len);

			std::string right       = trim(filter.substr(splitter + compare_len, right_end - splitter - compare_len));

			if (right[0] == '"') {
				std::size_t dq  = filter.find_first_of("\"", splitter + 1 + compare_len);

				right           = filter.substr(splitter + 1 + compare_len, dq - splitter - (1 + compare_len));

				right_end       = dq + 1;
			}

			auto        field       = m.get_meta().find(left);

			bool        cur_rc      = false;

			if (field == m.get_meta().end()) {
				if (ignore_if_field_is_missing == false)
					error_exit(false, "Filter (%s): field \"%s\" not found", filter.c_str(), left.c_str());

				cur_rc = true;
			}
			else {
				std::string value = field->second.s_value;

				cur_rc            = equals ? value == right : value != right;
			}

			if (operation.empty())
				rc = cur_rc;
			else if (operation == "&&")
				rc = rc && cur_rc;
			else if (operation == "||")
				rc = rc || cur_rc;
			else
				error_exit(false, "Unknown operation \"%s\"", operation.c_str());

			position = right_end;

			operation.clear();
		}
	}

	return rc;
}
