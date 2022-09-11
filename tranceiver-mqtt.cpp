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


#if MOSQUITTO_FOUND == 1
void on_mqtt_message(mosquitto *mi, void *user, const mosquitto_message *msg)
{
	tranceiver_mqtt *t = reinterpret_cast<tranceiver_mqtt *>(user);

        uint64_t msg_id = get_random_uint64_t();

	timeval tv;
	gettimeofday(&tv, nullptr);

        message m(tv,
                        myformat("MQTT(%s)", t->get_id().c_str()),
                        msg_id,
                        reinterpret_cast<uint8_t *>(msg->payload),
                        msg->payloadlen);

	t->mlog(LL_DEBUG, m, "on_mqtt_message", "Transmit msg from MQTT: " + std::string(reinterpret_cast<const char *>(msg->payload), msg->payloadlen));

        t->queue_incoming_message(m);
}

mosquitto *init_mqtt(tranceiver *const t, const std::string & mqtt_host, const int mqtt_port, const std::string & topic_in)
{
	log(LL_INFO, "Initializing MQTT");

	int err = 0;

	mosquitto *mi = mosquitto_new(nullptr, true, t);
	if (!mi)
		error_exit(false, "init_mqtt: Cannot crate mosquitto instance");

	if ((err = mosquitto_connect(mi, mqtt_host.c_str(), mqtt_port, 30)) != MOSQ_ERR_SUCCESS)
		error_exit(false, "mqtt failed to connect to [%s]:%d (%s)", mqtt_host.c_str(), mqtt_port, mosquitto_strerror(err));

	mosquitto_message_callback_set(mi, on_mqtt_message);

	if (topic_in.empty() == false) {
		if ((err = mosquitto_subscribe(mi, nullptr, topic_in.c_str(), 0)) != MOSQ_ERR_SUCCESS)
			error_exit(false, "mqtt failed to subscribe to topic \"%s\" (%s)", topic_in.c_str(), mosquitto_strerror(err));
	}

	if ((err = mosquitto_loop_start(mi)) != MOSQ_ERR_SUCCESS)
		error_exit(false, "mqtt failed to start thread (%s)", mosquitto_strerror(err));

	return mi;
}
#endif

transmit_error_t tranceiver_mqtt::put_message_low(const message & m)
{
	auto content = m.get_content();

	int err = 0;

#if MOSQUITTO_FOUND == 1
	if (topic_out.empty() == false) {
		if ((err = mosquitto_publish(mi, nullptr, topic_out.c_str(), content.second, content.first, 0, false)) != MOSQ_ERR_SUCCESS) {
			mlog(LL_WARNING, m, "put_message_low", myformat("mqtt failed to publish: %s", mosquitto_strerror(err)));

			return TE_hardware;
		}
	}

	if (topic_out_json.empty() == false) {
		std::string json = message_to_json(m);

		if ((err = mosquitto_publish(mi, nullptr, topic_out_json.c_str(), json.size(), json.c_str(), 0, false)) != MOSQ_ERR_SUCCESS) {
			mlog(LL_WARNING, m, "put_message_low", myformat("mqtt failed to publish (json): %s", mosquitto_strerror(err)));

			return TE_hardware;
		}
	}
#endif

	return TE_ok;
}

tranceiver_mqtt::tranceiver_mqtt(const std::string & id, seen *const s, work_queue_t *const w, gps_connector *const gps, const std::string & mqtt_host, const int mqtt_port, const std::string & topic_in, const std::string & topic_out, const std::string & topic_out_json) :
	tranceiver(id, s, w, gps),
	topic_in(topic_in), topic_out(topic_out), topic_out_json(topic_out_json)
{
#if MOSQUITTO_FOUND == 1
	log(LL_INFO, "Instantiated MQTT");

	mi = init_mqtt(this, mqtt_host, mqtt_port, topic_in);
#endif
}

tranceiver_mqtt::~tranceiver_mqtt()
{
#if MOSQUITTO_FOUND == 1
	mosquitto_destroy(mi);
#endif
}

void tranceiver_mqtt::operator()()
{
}

tranceiver *tranceiver_mqtt::instantiate(const libconfig::Setting & node_in, work_queue_t *const w, gps_connector *const gps)
{
#if MOSQUITTO_FOUND == 1
	std::string  id;
	seen        *s { nullptr };
	std::string  mqtt_host;
	std::string  topic_in;
	std::string  topic_out;
	std::string  topic_out_json;
	int          mqtt_port { 1883 };

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "repetition-rate-limiting") {
			if (s)
				error_exit(false, "(line %d): repetition-rate-limiting already defined", node.getSourceLine());

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
		else if (type == "topic-out-json")
			topic_out_json = node_in.lookup(type).c_str();
		else if (type != "type") {
			error_exit(false, "(line %d): MQTT setting \"%s\" is not known", node.getSourceLine(), type.c_str());
		}
        }

	if (mqtt_host.empty())
		error_exit(true, "(line %d): No MQTT server selected", node_in.getSourceLine());

	return new tranceiver_mqtt(id, s, w, gps, mqtt_host, mqtt_port, topic_in, topic_out, topic_out_json);
#else
	error_exit(false, "(line %d): libmosquitto not compiled in", node_in.getSourceLine());
#endif
}
