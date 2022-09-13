#include <string>

#include "db.h"
#include "stats.h"

void * start_webserver(const int listen_port, const std::string & ws_url, const int ws_port, stats *const s, db *const d, configuration *const cfg);
void stop_webserver(void *d);
