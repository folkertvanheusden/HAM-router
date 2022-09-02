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
#include "tranceiver-mqtt.h"
#include "utils.h"


mosquitto *init_mqtt(const std::string & mqtt_host, const int mqtt_port)
{
	log(LL_INFO, "Initializing MQTT");

	int err = 0;

	mosquitto *mi = mosquitto_new(nullptr, true, nullptr);
	if (!mi)
		error_exit(false, "Cannot crate mosquitto instance");

	if ((err = mosquitto_connect(mi, mqtt_host.c_str(), mqtt_port, 30)) != MOSQ_ERR_SUCCESS)
		error_exit(false, "mqtt failed to connect (%s)", mosquitto_strerror(err));

	if ((err = mosquitto_loop_start(mi)) != MOSQ_ERR_SUCCESS)
		error_exit(false, "mqtt failed to start thread (%s)", mosquitto_strerror(err));

	return mi;
}

transmit_error_t tranceiver_mqtt::put_message_low(const message & m)
{
	auto content = m.get_content();

	int err = 0;
	if ((err = mosquitto_publish(mi, nullptr, topic_out.c_str(), content.second, content.first, 0, false)) != MOSQ_ERR_SUCCESS) {
		log(LL_WARNING, "mqtt failed to publish %s: %s", m.get_id_short().c_str(), mosquitto_strerror(err));

		return TE_hardware;
	}

	return TE_ok;
}

tranceiver_mqtt::tranceiver_mqtt(const std::string & id, seen *const s, work_queue_t *const w, mosquitto *const mi, const std::string & topic_in, const std::string & topic_out) :
	tranceiver(id, s, w),
	mi(mi),
	topic_in(topic_in), topic_out(topic_out)
{
	log(LL_INFO, "Instantiated MQTT (%s)", id.c_str());
}

tranceiver_mqtt::~tranceiver_mqtt()
{
	mosquitto_destroy(mi);
}

void tranceiver_mqtt::operator()()
{
	// TODO
}

tranceiver *tranceiver_mqtt::instantiate(const libconfig::Setting & node_in, work_queue_t *const w)
{
	std::string  id;
	seen        *s { nullptr };
	std::string  mqtt_host;
	std::string  topic_in;
	std::string  topic_out;
	int          mqtt_port { 1883 };

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "incoming-rate-limiting") {
			assert(s == nullptr);
			s = seen::instantiate(node);
		}
		else if (type == "host")
			mqtt_host = node_in.lookup(type).c_str();
		else if (type == "port")
			mqtt_port = node_in.lookup(type);
		else if (type == "topic-in")
			topic_in = node_in.lookup(type).c_str();
		else if (type == "topic-out")
			topic_out = node_in.lookup(type).c_str();
		else if (type != "type") {
			error_exit(false, "MQTT setting \"%s\" is now known", type.c_str());
		}
        }

	if (mqtt_host.empty())
		error_exit(true, "No MQTT server selected");

	mosquitto *mi = init_mqtt(mqtt_host, mqtt_port);

	return new tranceiver_mqtt(id, s, w, mi, topic_in, topic_out);
}
