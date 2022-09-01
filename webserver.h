#include <string>

#include "stats.h"


void * start_webserver(const int listen_port, const std::string & ws_url, const int ws_port, stats *const s);
void stop_webserver(void *d);
