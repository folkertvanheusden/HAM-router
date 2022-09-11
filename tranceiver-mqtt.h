#include "config.h"
#if MOSQUITTO_FOUND == 1
#include <mosquitto.h>
#endif

#include "db.h"
#include "tranceiver.h"


class tranceiver_mqtt : public tranceiver
{
private:
#if MOSQUITTO_FOUND == 1
	mosquitto  *mi        { nullptr };
#endif

	std::string topic_in;
	std::string topic_out;
	std::string topic_out_json;

protected:
	transmit_error_t put_message_low(const message & m) override;

public:
	tranceiver_mqtt(const std::string & id, seen *const s, work_queue_t *const w, gps_connector *const gps, const std::string & mqtt_host, const int mqtt_port, const std::string & topic_in, const std::string & topic_out, const std::string & topic_out_json);
	virtual ~tranceiver_mqtt();

	std::string get_type_name() const override { return "MQTT"; }

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w, gps_connector *const gps);

	void operator()() override;
};
