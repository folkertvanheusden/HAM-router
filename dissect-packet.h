#include <map>
#include <optional>
#include <stdint.h>
#include <string>

#include "ax25.h"
#include "db-common.h"


std::optional<std::pair<std::map<std::string, db_record_data>, ax25 *> > dissect_packet(const uint8_t *const p, const size_t size);
