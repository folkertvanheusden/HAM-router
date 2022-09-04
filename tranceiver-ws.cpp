#include <assert.h>
#include <errno.h>
#include <optional>
#include <string>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "log.h"
#include "net.h"
#include "random.h"
#include "str.h"
#include "tranceiver-ws.h"
#include "utils.h"


transmit_error_t tranceiver_ws::put_message_low(const message & m)
{
	push_to_websockets(ws, m);

	return TE_ok;
}

tranceiver_ws::tranceiver_ws(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos, ws_global_context_t *const ws) :
	tranceiver(id, s, w, pos),
	ws(ws)
{
	log(LL_INFO, "Instantiated websockets (%s)", id.c_str());
}

tranceiver_ws::~tranceiver_ws()
{
}

void tranceiver_ws::operator()()
{
}

tranceiver *tranceiver_ws::instantiate(const libconfig::Setting & node_in, work_queue_t *const w, const position_t & pos, ws_global_context_t *const ws)
{
	std::string  id;
	seen        *s  { nullptr };

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "incoming-rate-limiting") {
			assert(s == nullptr);
			s = seen::instantiate(node);
		}
		else if (type != "type") {
			error_exit(false, "Websocket setting \"%s\" is not known", type.c_str());
		}
        }

	return new tranceiver_ws(id, s, w, pos, ws);
}
