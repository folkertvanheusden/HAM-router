#include <map>
#include <optional>
#include <stdint.h>
#include <string>

#include "db-common.h"


std::optional<std::map<std::string, db_record_data> > dissect_packet(const uint8_t *const p, const size_t size);
