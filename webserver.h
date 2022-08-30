#include <string>

#include "db.h"
#include "stats.h"

void * start_webserver(const int listen_port, const std::string & ws_virtual_host, const int ws_port, stats *const s, db *const d);
void stop_webserver(void *d);
