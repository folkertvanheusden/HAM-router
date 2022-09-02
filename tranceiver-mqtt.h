#include <mosquitto.h>

#include "db.h"
#include "tranceiver.h"


class tranceiver_mqtt : public tranceiver
{
private:
	mosquitto  *mi        { nullptr };

	std::string topic_in;
	std::string topic_out;

protected:
	transmit_error_t put_message_low(const message & m) override;

public:
	tranceiver_mqtt(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos, const std::string & mqtt_host, const int mqtt_port, const std::string & topic_in, const std::string & topic_out);
	virtual ~tranceiver_mqtt();

	std::string get_type_name() const override { return "MQTT"; }

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w, const position_t & pos);

	void operator()() override;
};
