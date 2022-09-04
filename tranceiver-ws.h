#include <mosquitto.h>

#include "tranceiver.h"
#include "websockets.h"


class tranceiver_ws : public tranceiver
{
private:
	ws_global_context_t *const ws;

protected:
	transmit_error_t put_message_low(const message & m) override;

public:
	tranceiver_ws(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos, ws_global_context_t *const ws);
	virtual ~tranceiver_ws();

	std::string get_type_name() const override { return "WebSockets"; }

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w, const position_t & pos, ws_global_context_t *const ws);

	void operator()() override;
};
