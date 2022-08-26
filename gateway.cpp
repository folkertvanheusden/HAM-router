#include <atomic>
#include <condition_variable>
#include <errno.h>
#include <jansson.h>
#include <mosquitto.h>
#include <mutex>
#include <poll.h>
#include <pthread.h>
#include <pty.h>
#include <queue>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <thread>
#include <unistd.h>

extern "C" {
#include "LoRa.h"
}

#include "aprs-si.h"
#include "db.h"
#include "error.h"
#include "gps.h"
#include "ini.h"
#include "kiss.h"
#include "log.h"
#include "net.h"
#include "snmp.h"
#include "stats.h"
#include "utils.h"
#include "webserver.h"
#include "websockets.h"

std::string callsign;
std::string aprs_user;
std::string aprs_pass;
double local_lat = 0;
double local_lng = 0;
std::string db_url;
std::string db_user;
std::string db_pass;
std::string logfile;
int gpio_lora_reset = 0;
int gpio_lora_dio0  = 0;
bool        local_ax25 = true;
std::string if_up;
std::string mqtt_host;
int         mqtt_port = -1;
std::string mqtt_aprs_packet_meta;
std::string mqtt_aprs_packet_as_is;
std::string mqtt_ax25_packet_meta;
std::string mqtt_ax25_packet_as_is;
std::string syslog_host;
int         syslog_port = -1;
int         ws_port       = -1;
bool        ws_ssl_enable = false;
std::string ws_ssl_cert;
std::string ws_ssl_priv_key;
std::string ws_ssl_ca;
int         snmp_port = -1;
int         http_port = -1;
int         beacon_interval = 0;

std::atomic_bool terminate { false };

std::queue<rxData>      packets;
std::mutex              packets_lock;
std::condition_variable packets_cv;

void signal_handler(int sig)
{
	log(LL_INFO, "Terminate requested");

	terminate = true;

	packets_cv.notify_all();
}

#define INI_MATCH(s, n) (strcmp(section, s) == 0 && strcmp(name, n) == 0)

int handler_ini(void* user, const char* section, const char* name, const char* value)
{
	if (INI_MATCH("general", "callsign")) {
		callsign = value;
	}
	else if (INI_MATCH("general", "local-latitude")) {
		local_lat = atof(value);
	}
	else if (INI_MATCH("general", "local-longitude")) {
		local_lng = atof(value);
	}
	else if (INI_MATCH("general", "logfile")) {
		logfile = value;
	}
	else if (INI_MATCH("general", "local-ax25")) {
		local_ax25 = strcasecmp(value, "true") == 0;
	}
	else if (INI_MATCH("general", "if-up")) {
		if_up = value;
	}
	else if (INI_MATCH("general", "beacon-interval")) {
		beacon_interval = atoi(value);
	}
	else if (INI_MATCH("aprsi", "user")) {
		aprs_user = value;
	}
	else if (INI_MATCH("aprsi", "password")) {
		aprs_pass = value;
	}
	else if (INI_MATCH("db", "url")) {
		db_url = value;
	}
	else if (INI_MATCH("db", "user")) {
		db_user = value;
	}
	else if (INI_MATCH("db", "password")) {
		db_pass = value;
	}
	else if (INI_MATCH("tranceiver", "reset-pin")) {
		gpio_lora_reset = atoi(value);
	}
	else if (INI_MATCH("tranceiver", "dio0-pin")) {
		gpio_lora_dio0 = atoi(value);
	}
	else if (INI_MATCH("mqtt", "host")) {
		mqtt_host = value;
	}
	else if (INI_MATCH("mqtt", "port")) {
		mqtt_port = atoi(value);
	}
	else if (INI_MATCH("mqtt", "aprs-packet-meta-topic")) {
		mqtt_aprs_packet_meta = value;
	}
	else if (INI_MATCH("mqtt", "aprs-packet-as-is-topic")) {
		mqtt_aprs_packet_as_is = value;
	}
	else if (INI_MATCH("mqtt", "ax25-packet-meta-topic")) {
		mqtt_ax25_packet_meta = value;
	}
	else if (INI_MATCH("mqtt", "ax25-packet-as-is-topic")) {
		mqtt_ax25_packet_as_is = value;
	}
	else if (INI_MATCH("syslog", "host")) {
		syslog_host = value;
	}
	else if (INI_MATCH("syslog", "port")) {
		syslog_port = atoi(value);
	}
	else if (INI_MATCH("websockets", "port")) {
		ws_port = atoi(value);
	}
	else if (INI_MATCH("websockets", "enable-ssl")) {
		ws_ssl_enable = strcasecmp(value, "true") == 0;
	}
	else if (INI_MATCH("websockets", "certificate")) {
		ws_ssl_cert = value;
	}
	else if (INI_MATCH("websockets", "private-key")) {
		ws_ssl_priv_key = value;
	}
	else if (INI_MATCH("websockets", "ca")) {
		ws_ssl_ca = value;
	}
	else if (INI_MATCH("snmp", "port")) {
		snmp_port = atoi(value);
	}
	else if (INI_MATCH("http", "port")) {
		http_port = atoi(value);
	}
	else {
		return 0;
	}

	return 1;
}

void load_ini_file(const std::string & file)
{
	if (ini_parse(file.c_str(), handler_ini, nullptr) < 0)
		error_exit(true, "Cannot process INI-file \"%s\"", file.c_str());
}

void tx_f(txData *tx)
{
	log(LL_DEBUG, "transmitted");
}

void lora_transmit(LoRa_ctl *const modem, std::mutex *const modem_lock, const uint8_t *const what, const int len, db *const d)
{
	if (len > 255) {
		log(LL_WARNING, "lora_transmit: packet too big (%d bytes)", len);

		return;
	}

	modem_lock->lock();

	memcpy(modem->tx.data.buf, what, len);

	log(LL_DEBUG, "transmit: %s", dump_hex(reinterpret_cast<const uint8_t *>(modem->tx.data.buf), len).c_str());

	LoRa_stop_receive(modem); //manually stoping RxCont mode

	while(LoRa_get_op_mode(modem) != STDBY_MODE)
		usleep(101000);

	modem->tx.data.size = len;

	LoRa_send(modem);

	while(LoRa_get_op_mode(modem) != STDBY_MODE)
		usleep(101000);

	log(LL_DEBUG, "Time on air data - Tsym: %f; Tpkt: %f; payloadSymbNb: %u", modem->tx.data.at.Tsym, modem->tx.data.at.Tpkt, modem->tx.data.at.payloadSymbNb);

	LoRa_receive(modem);

	modem_lock->unlock();

	d->insert_airtime(modem->tx.data.at.Tpkt, true, callsign);
}

void * rx_f(void *in)
{
	rxData *rx = reinterpret_cast<rxData *>(in);
	if (rx->size == 0 || rx->CRC)
		return NULL;

	packets_lock.lock();
	packets.push(*rx);
	packets_cv.notify_one();
	packets_lock.unlock();

	free(rx);

	return NULL;
}

void process_incoming(const int kiss_fd, struct mosquitto *const mi, const int ws_port, stats *const s, aprs_si *as, db *const d, LoRa_ctl *const modem)
{
	log(LL_INFO, "Starting \"LoRa APRS -> aprsi/mqtt/syslog/db\"-thread");

	int fd = -1;

	ws_global_context_t ws;
	ws.ts = 0;

	if (ws_port != -1)
		start_websocket_thread(ws_port, &ws, ws_ssl_enable, ws_ssl_cert, ws_ssl_priv_key, ws_ssl_ca);

        uint64_t *phys_ifInOctets     = s->register_stat("phys_ifInOctets",     myformat("1.3.6.1.2.1.2.2.1.10.%zu", 1),    snmp_integer::si_counter32);
        uint64_t *phys_ifHCInOctets   = s->register_stat("phys_ifHCInOctets",   myformat("1.3.6.1.2.1.31.1.1.1.6.%zu", 1),  snmp_integer::si_counter64);

	uint64_t *lora_ifOutOctets    = s->register_stat("lora_ifOutOctets",    myformat("1.3.6.1.2.1.2.2.1.16.%zu", 2),    snmp_integer::si_counter32);
	uint64_t *lora_ifHCOutOctets  = s->register_stat("lora_ifHCOutOctets",  myformat("1.3.6.1.2.1.31.1.1.1.10.%zu", 2), snmp_integer::si_counter64);

        uint64_t *cnt_frame_ax25       = s->register_stat("cnt_frame_ax25", "1.3.6.1.2.1.4.57850.2.1.1");  // 1.3.6.1.2.1.4.57850.2.1. packet type counts
        uint64_t *cnt_frame_aprs       = s->register_stat("cnt_frame_aprs", "1.3.6.1.2.1.4.57850.2.1.2");
        uint64_t *cnt_aprs_invalid_loc = s->register_stat("cnt_aprs_invalid_loc", "1.3.6.1.2.1.4.57850.2.2.1");  // 1.3.6.1.2.1.4.57850.2.2 aprs counters
        uint64_t *cnt_aprs_invalid_cs  = s->register_stat("cnt_aprs_invalid_cs",  "1.3.6.1.2.1.4.57850.2.2.2");
        //uint64_t *cnt_ax25_invalid_cs  = s->register_stat("cnt_ax25_invalid_cs",  "1.3.6.1.2.1.4.57850.2.3.2");  // 1.3.6.1.2.1.4.57850.3.2 ax25 counters

        uint64_t *cnt_aprsi_failures   = s->find_stat("cnt_aprsi_failures");

	for(;!terminate;) {
		try {
			pthread_setname_np(pthread_self(), "tx_thread");

			std::unique_lock<std::mutex> lck(packets_lock);

			while(packets.empty() && !terminate)
				packets_cv.wait(lck);

			rxData rx = packets.front();
			packets.pop();

			lck.unlock();

			if (terminate)
				break;

			stats_add_counter(phys_ifInOctets,   rx.size);
			stats_add_counter(phys_ifHCInOctets, rx.size);

			stats_add_counter(lora_ifOutOctets,   rx.size);
			stats_add_counter(lora_ifHCOutOctets, rx.size);

			const uint8_t *const data = reinterpret_cast<const uint8_t *>(rx.buf);

			json_t     *meta         = nullptr;
			const char *meta_str     = nullptr;
			int         meta_str_len = 0;

			std::string to;
			std::string to_full;  // only relevant for OE_
			std::string from;
			std::string content_out = reinterpret_cast<const char *>(&data[3]);  // for OE_ only

			bool        oe_ = false;

			if (data[0] == 0x3c && data[1] == 0xff && data[2] == 0x01) {  // OE_
				const char *const gt = strchr(&rx.buf[3], '>');
				if (gt) {
					const char *const colon = strchr(gt, ':');
					if (colon) {
						to_full = std::string(gt + 1, colon - gt - 1);

						std::size_t delimiter = to.find(',');

						if (delimiter != std::string::npos)
							to = to_full.substr(0, delimiter);
						else
							to = to_full;

						from    = std::string(&rx.buf[3], gt - rx.buf - 3);

						content_out = from + ">" + to_full + ",qAO," + callsign + colon;
					}
					else {
						stats_inc_counter(cnt_aprs_invalid_cs);
					}
				}
				else {
					stats_inc_counter(cnt_aprs_invalid_cs);
				}

				oe_ = true;

				stats_inc_counter(cnt_frame_aprs);
			}
			else {  // assuming AX.25
				to   = get_ax25_addr(&data[0]);
				from = get_ax25_addr(&data[7]);

				stats_inc_counter(cnt_frame_ax25);
			}

			if (from.empty())
				d->insert_airtime(modem->tx.data.at.Tpkt, false, { });
			else
				d->insert_airtime(modem->tx.data.at.Tpkt, false, from);

			double latitude = 0, longitude = 0, distance = -1.0;

			char *colon = strchr(rx.buf, ':');
			if (colon && rx.size - (rx.buf - colon) >= 7) {
				parse_nmea_pos(colon + 1, &latitude, &longitude);

				if (latitude != 0. || longitude != 0.)
					distance = calcGPSDistance(latitude, longitude, local_lat, local_lng);
				else
					stats_inc_counter(cnt_aprs_invalid_loc);
			}
			else {
				stats_inc_counter(cnt_aprs_invalid_loc);
			}

			log(LL_INFO, "timestamp: %u%06u, CRC error: %d, RSSI: %d, SNR: %f (%f,%f => distance: %.2fm) %s => %s (%s)", rx.last_time.tv_sec, rx.last_time.tv_usec, rx.CRC, rx.RSSI, rx.SNR, latitude, longitude, distance, from.c_str(), to_full.c_str(), oe_ ? "OE" : "AX.25");

			if (mi && (mqtt_aprs_packet_meta.empty() == false || mqtt_ax25_packet_meta.empty() == false || syslog_host.empty() == false || ws_port != -1)) {
				meta = json_object();

				json_object_set(meta, "timestamp", json_integer(rx.last_time.tv_sec));

				json_object_set(meta, "CRC-error", json_integer(rx.CRC));

				json_object_set(meta, "RSSI", json_real(double(rx.RSSI)));

				json_object_set(meta, "SNR", json_real(rx.SNR));

				if (latitude != 0. || longitude != 0.) {
					json_object_set(meta, "latitude", json_real(latitude));

					json_object_set(meta, "longitude", json_real(longitude));

					if (distance >= 0.)
						json_object_set(meta, "distance", json_real(distance));
				}

				if (to.empty() == false)
					json_object_set(meta, "callsign-to", json_string(to.c_str()));

				if (from.empty() == false)
					json_object_set(meta, "callsign-from", json_string(from.c_str()));

				json_object_set(meta, "data", json_string(dump_replace(reinterpret_cast<const uint8_t *>(rx.buf), rx.size).c_str()));

				meta_str     = json_dumps(meta, 0);
				meta_str_len = strlen(meta_str);
			}

			if (d)
				d->insert_message(reinterpret_cast<uint8_t *>(rx.buf), rx.size, rx.RSSI, rx.SNR, rx.CRC, latitude, longitude, distance, to, from);

			if (mi && mqtt_aprs_packet_meta.empty() == false) {
				int err = 0;
				if ((err = mosquitto_publish(mi, nullptr, mqtt_aprs_packet_meta.c_str(), meta_str_len, meta_str, 0, false)) != MOSQ_ERR_SUCCESS)
					log(LL_WARNING, "mqtt failed to publish (%s)", mosquitto_strerror(err));
			}

			if (syslog_host.empty() == false)
				transmit_udp(syslog_host, syslog_port, reinterpret_cast<const uint8_t *>(meta_str), meta_str_len);

			if (ws_port != -1)
				push_to_websockets(&ws, meta_str);

			if (oe_) {  // OE_
				if (aprs_user.empty() == false) {
					if (!as->send_through_aprs_is(content_out))
						stats_inc_counter(cnt_aprsi_failures);
				}

				if (mi && mqtt_aprs_packet_as_is.empty() == false) {
					int err = 0;
					if ((err = mosquitto_publish(mi, nullptr, mqtt_aprs_packet_as_is.c_str(), rx.size, data, 0, false)) != MOSQ_ERR_SUCCESS)
						log(LL_WARNING, "mqtt failed to publish (%s)", mosquitto_strerror(err));
				}
			}
			else {
				if (kiss_fd != -1)
					send_mkiss(kiss_fd, 0, data, rx.size);

				if (mi && mqtt_ax25_packet_as_is.empty() == false) {
					int err = 0;
					if ((err = mosquitto_publish(mi, nullptr, mqtt_ax25_packet_as_is.c_str(), rx.size, data, 0, false)) != MOSQ_ERR_SUCCESS)
						log(LL_WARNING, "mqtt failed to publish (%s)", mosquitto_strerror(err));
				}

				if (mi && mqtt_ax25_packet_meta.empty() == false) {
					int err = 0;
					if ((err = mosquitto_publish(mi, nullptr, mqtt_ax25_packet_meta.c_str(), meta_str_len, meta_str, 0, false)) != MOSQ_ERR_SUCCESS)
						log(LL_WARNING, "mqtt failed to publish (%s)", mosquitto_strerror(err));
				}
			}

			if (meta) {
				json_decref(meta);

				free((void *)meta_str);
			}
		}
		catch(const std::exception& e) {
			log(LL_ERR, "process_incoming: exception %s", e.what());
		}
	}

	if (fd != -1)
		close(fd);
}

void init_modem(LoRa_ctl *const modem)
{
	memset(modem, 0x00, sizeof *modem);

	// these settings are specific for APRS over LoRa

	modem->spiCS = 0;                         // Raspberry SPI CE pin number
	modem->rx.callback = rx_f;
	modem->tx.callback = tx_f;
	modem->eth.preambleLen = 8;
	modem->eth.bw = BW125;                    // Bandwidth 125KHz
	modem->eth.sf = SF12;                     // Spreading Factor 12
	modem->eth.ecr = CR5;                     // Error coding rate CR4/8
	modem->eth.freq = 433775000;              // 434.8MHz
	modem->eth.resetGpioN = gpio_lora_reset;  // tranceiver reset pin
	modem->eth.dio0GpioN = gpio_lora_dio0;
	modem->eth.outPower = OP20;               // Output power
	modem->eth.powerOutPin = PA_BOOST;        // Power Amplifire pin
	modem->eth.AGC = 1;                       // enable Auto Gain Control
	modem->eth.OCP = 240;                     // 45 to 240 mA. 0 to turn off protection
	modem->eth.implicitHeader = 0;            // select "explicit header" mode
	modem->eth.syncWord = 0x12;
	modem->eth.CRC = 1;

	LoRa_begin(modem);

	LoRa_receive(modem);
}

void configure_ax25_interface(int *const fd, std::string *const device_name)
{
	int fd_master = -1;
	int fd_slave  = -1;

	log(LL_INFO, "Configuring local AX.25 interface");

	if (openpty(&fd_master, &fd_slave, NULL, NULL, NULL) == -1)
		error_exit(true, "openpty failed");

	int disc = N_AX25;
	if (ioctl(fd_slave, TIOCSETD, &disc) == -1)
		error_exit(true, "error setting line discipline");

	if (setifcall(fd_slave, callsign.c_str()) == -1)
		error_exit(false, "cannot set call");

	int v = 4;
	if (ioctl(fd_slave, SIOCSIFENCAP, &v) == -1)
		error_exit(true, "failed to set encapsulation");

	char dev_name[64] = { 0 };
	if (ioctl(fd_slave, SIOCGIFNAME, dev_name) == -1)
		error_exit(true, "failed retrieving name of ax25 network device name");

	startiface(dev_name);

	if (if_up.empty() == false)
		system((if_up + " " + dev_name).c_str());

	*fd = fd_master;

	device_name->assign(dev_name);
}

void send_beacons(LoRa_ctl *const modem, std::mutex *const modem_lock, const int beacon_interval, stats *const s, aprs_si *const as, db *const d)
{
	if (beacon_interval <= 0)
		return;

	log(LL_INFO, "Starting beacon transmitter (to LoRa & APRS-IS)");

	if (beacon_interval < 600)
		error_exit(false, "Beacon interval should be at least 10 minutes");

	uint64_t *cnt_aprsi_failures = s->find_stat("cnt_aprsi_failures");

	uint64_t *phys_ifOutOctets   = s->find_stat("phys_ifOutOctets");
	uint64_t *phys_ifHCOutOctets = s->find_stat("phys_ifHCOutOctets");

	uint64_t *lora_ifInOctets    = s->find_stat("lora_ifInOctets");
	uint64_t *lora_ifHCInOctets  = s->find_stat("lora_ifHCInOctets");

	for(;!terminate;) {
		try {
			log(LL_DEBUG, "Queueing beacon-message for APRS-FI");
			std::string message = "=" + gps_double_to_aprs(local_lat, local_lng) + "&LoRa APRS/AX.25 gateway, https://github.com/folkertvanheusden/lora-aprs-gw";

			// send to APRS-IS
			std::string beacon_aprs_is = callsign + "-L>APLG01,TCPIP*,qAC:" + message;

			if (aprs_user.empty() == false) {
				if (!as->send_through_aprs_is(beacon_aprs_is))
					stats_inc_counter(cnt_aprsi_failures);
			}

			// send to RF
			std::string beacon_rf = ">\xff\x01" + message;
			size_t beacon_rf_len = beacon_rf.size();

			log(LL_DEBUG, "Sending beacon via RF");
			lora_transmit(modem, modem_lock, reinterpret_cast<const uint8_t *>(beacon_rf.c_str()), beacon_rf_len, d);

			stats_add_counter(phys_ifOutOctets,   beacon_rf_len);
			stats_add_counter(phys_ifHCOutOctets, beacon_rf_len);

			stats_add_counter(lora_ifInOctets,   beacon_rf_len);
			stats_add_counter(lora_ifHCInOctets, beacon_rf_len);

			log(LL_DEBUG, "Sleeping %d seconds for next beacon", beacon_interval);
		}
		catch(const std::exception& e) {
			log(LL_ERR, "beacon: exception %s", e.what());
		}

		for(int i=0; i<beacon_interval * 10 && !terminate; i++)
			usleep(100000);
	}
}

mosquitto *init_mqtt(const std::string & mqtt_host, const int port)
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

int main(int argc, char *argv[])
{
	if (argc != 2)
		error_exit(false, "Requires filename of configuration ini-file");

	load_ini_file(argv[1]);

	setlogfile(logfile.c_str(), LL_DEBUG);

	log(LL_INFO, "Configuration-file loaded");

	db *d = nullptr;

	if (db_url.empty() == false)
		d = new db(db_url, db_user, db_pass);

	std::mutex modem_lock;
	LoRa_ctl   modem;
	init_modem(&modem);

	signal(SIGINT, signal_handler);

	int         kiss_fd          = -1;
	std::string ax25_device_name;

	if (local_ax25)
		configure_ax25_interface(&kiss_fd, &ax25_device_name);

	aprs_si as(aprs_user, aprs_pass);

	snmp_data_type_running_since running_since;

	snmp_data sd;
	sd.register_oid("1.3.6.1.2.1.1.1.0", "lora_aprs_gw");
	sd.register_oid("1.3.6.1.2.1.1.2.0", new snmp_data_type_oid("1.3.6.1.2.1.4.57850.2"));
	sd.register_oid("1.3.6.1.2.1.1.3.0", &running_since);
	sd.register_oid("1.3.6.1.2.1.1.4.0", "Folkert van Heusden <mail@vanheusden.com>");
	sd.register_oid("1.3.6.1.2.1.1.5.0", "lora_aprs_gw");
	sd.register_oid("1.3.6.1.2.1.1.6.0", "Earth");
	sd.register_oid("1.3.6.1.2.1.1.7.0", snmp_integer::si_integer, 254 /* everything but the physical layer */);
	sd.register_oid("1.3.6.1.2.1.1.8.0", snmp_integer::si_integer, 0);  // The value of sysUpTime at the time of the most recent change in state or value of any instance of sysORID.

	sd.register_oid("1.3.6.1.2.1.2.1.0", snmp_integer::si_integer, local_ax25 ? 2 : 1);  // number of network interfaces

	// register interface 1
	sd.register_oid(myformat("1.3.6.1.2.1.2.2.1.1.%zu", 1), snmp_integer::si_integer, 1);
	sd.register_oid(myformat("1.3.6.1.2.1.31.1.1.1.1.%zu", 1), ax25_device_name);  // name
	sd.register_oid(myformat("1.3.6.1.2.1.2.2.1.2.1.%zu",  1), "network interface");  // description
	sd.register_oid(myformat("1.3.6.1.2.1.17.1.4.1.%zu",   1), snmp_integer::si_integer, 1);  // device is up (1)

	sd.register_oid(myformat("1.3.6.1.2.1.2.2.1.1.%zu", 2), snmp_integer::si_integer, 2);
	sd.register_oid(myformat("1.3.6.1.2.1.31.1.1.1.1.%zu", 2), "LoRa");  // name
	sd.register_oid(myformat("1.3.6.1.2.1.2.2.1.2.1.%zu",  2), "LoRa tranceiver");  // description
	sd.register_oid(myformat("1.3.6.1.2.1.17.1.4.1.%zu",   2), snmp_integer::si_integer, 1);  // device is up (1)

	stats s(8192, &sd);

	uint64_t *phys_ifOutOctets    = s.register_stat("phys_ifOutOctets",    myformat("1.3.6.1.2.1.2.2.1.16.%zu", 1),    snmp_integer::si_counter32);
	uint64_t *phys_ifHCOutOctets  = s.register_stat("phys_ifHCOutOctets",  myformat("1.3.6.1.2.1.31.1.1.1.10.%zu", 1), snmp_integer::si_counter64);

	uint64_t *lora_ifInOctets     = s.register_stat("lora_ifInOctets",     myformat("1.3.6.1.2.1.2.2.1.10.%zu", 2),    snmp_integer::si_counter32);
	uint64_t *lora_ifHCInOctets   = s.register_stat("lora_ifHCInOctets",   myformat("1.3.6.1.2.1.31.1.1.1.6.%zu", 2),  snmp_integer::si_counter64);

        s.register_stat("cnt_aprsi_failures",  "1.3.6.1.2.1.4.57850.2.3.1");  // aprsi counters, used elsewhere

	snmp snmp_(&sd, &s, snmp_port);

	if (http_port != -1)
		start_webserver(http_port, ws_port, &s, d);

	mosquitto *mi = nullptr;

	if (mqtt_host.empty() == false)
		mi = init_mqtt(mqtt_host, mqtt_port);

	std::thread tx_thread([kiss_fd, mi, &s, &as, d, &modem] {
			process_incoming(kiss_fd, mi, ws_port, &s, &as, d, &modem);
		});

	std::thread beacon_thread([&modem, &modem_lock, &s, &as, d] {
			send_beacons(&modem, &modem_lock, beacon_interval, &s, &as, d);
		});

	if (local_ax25) {
		log(LL_INFO, "Starting transmit (local AX.25 stack to LoRa)");

		struct pollfd fds[] = { { kiss_fd, POLLIN, 0 } };

		for(;!terminate;) {
			int rc = poll(fds, 1, 100);

			if (rc == -1) {
				if (errno != EINTR) {
					log(LL_WARNING, "TERMINATE: (%s)", strerror(errno));
					break;
				}
			}

			if (rc == 0)
				continue;

			if (fds[0].revents) {
				uint8_t *p = NULL;
				int plen = 0;
				if (!recv_mkiss(kiss_fd, &p, &plen, true)) {
					log(LL_WARNING, "TERMINATE");
					break;
				}

				lora_transmit(&modem, &modem_lock, p, plen, d);

				stats_add_counter(phys_ifOutOctets,   plen);
				stats_add_counter(phys_ifHCOutOctets, plen);

				stats_add_counter(lora_ifInOctets,   plen);
				stats_add_counter(lora_ifHCInOctets, plen);

				free(p);
			}
		}
	}
	else {
		for(;;)
			sleep(86400);
	}

	LoRa_end(&modem);

	stop_webserver();

	stop_websockets();

	beacon_thread.join();

	tx_thread.join();

	delete d;

	log(LL_INFO, "END");
}
