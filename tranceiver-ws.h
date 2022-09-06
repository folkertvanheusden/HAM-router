#include <mosquitto.h>

#include "db.h"
#include "tranceiver.h"
#include "websockets.h"


class tranceiver_ws : public tranceiver
{
private:
	ws_global_context_t  ws;
	void                *webserver { nullptr };

protected:
	transmit_error_t put_message_low(const message & m) override;

public:
	tranceiver_ws(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos, const int http_port, const std::string & ws_url, const int ws_port, stats *const st, const bool ws_ssl_enabled, const std::string & ws_ssl_cert, const std::string & ws_ssl_priv_key, const std::string & ws_ssl_ca, db *const d);
	virtual ~tranceiver_ws();

	std::string get_type_name() const override { return "WebSockets"; }

	static tranceiver *instantiate(const libconfig::Setting & node_in, work_queue_t *const w, const position_t & pos, stats *const st, const std::vector<tranceiver *> & other_tranceivers);

	void operator()() override;
};
