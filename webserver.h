#include <string>

#include "db.h"
#include "stats.h"

void * start_webserver(const int listen_port, const int ws_port_in, stats *const s, db *const d);
void stop_webserver(void *d);
