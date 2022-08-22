#include <condition_variable>
#include <errno.h>
#include <jansson.h>
#include <math.h>
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

#include "db.h"
#include "error.h"
#include "ini.h"
#include "kiss.h"
#include "log.h"
#include "net.h"
#include "utils.h"

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
int gpio_lora_dio0 = 0;
bool local_ax25 = true;
std::string mqtt_host;
int         mqtt_port;
std::string mqtt_aprs_packet_meta;
std::string mqtt_aprs_packet_as_is;
std::string mqtt_ax25_packet_meta;
std::string mqtt_ax25_packet_as_is;

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

db *d = nullptr;

std::queue<rxData>      packets;
std::mutex              packets_lock;
std::condition_variable packets_cv;

void tx_f(txData *tx)
{
	log(LL_DEBUG, "transmitted");
}

// https://stackoverflow.com/questions/27126714/c-latitude-and-longitude-distance-calculator
#define RADIO_TERRESTRE 6372797.56085
#define GRADOS_RADIANES M_PI / 180.0
#define RADIANES_GRADOS 180.0 / M_PI

double calcGPSDistance(double latitude_new, double longitude_new, double latitude_old, double longitude_old)
{
    double  lat_new = latitude_old * GRADOS_RADIANES;
    double  lat_old = latitude_new * GRADOS_RADIANES;
    double  lat_diff = (latitude_new-latitude_old) * GRADOS_RADIANES;
    double  lng_diff = (longitude_new-longitude_old) * GRADOS_RADIANES;

    double  a = sin(lat_diff/2) * sin(lat_diff/2) +
                cos(lat_new) * cos(lat_old) *
                sin(lng_diff/2) * sin(lng_diff/2);
    double  c = 2 * atan2(sqrt(a), sqrt(1-a));

    double  distance = RADIO_TERRESTRE * c;

    return distance;
}

// from https://stackoverflow.com/questions/36254363/how-to-convert-latitude-and-longitude-of-nmea-format-data-to-decimal
double convertToDecimalDegrees(const char *latLon, const char direction)
{
	char deg[4] = { 0 };
	const char *dot = nullptr, *min = nullptr;
	int len;
	double dec = 0;

	if ((dot = strchr(latLon, '.')))
	{                                         // decimal point was found
		min = dot - 2;                          // mark the start of minutes 2 chars back
		len = min - latLon;                     // find the length of degrees
		strncpy(deg, latLon, len);              // copy the degree string to allow conversion to float
		dec = atof(deg) + atof(min) / 60;       // convert to float
		if (direction == 'S' || direction == 'W')
			dec *= -1;
	}

	return dec;
}

void parse_nmea_pos(const char *what, double *const lat, double *const lng)
{
	if (what[0] == '@') {  // ignore time code
		what += 7;
		// TODO
	}
	else if (what[0] == '!') {  // straight away position
		what++;

		*lat = convertToDecimalDegrees(what, what[6]);

		what += 9;
		*lng = convertToDecimalDegrees(what, what[6]);
	}
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

int main(int argc, char *argv[])
{
	if (argc != 2)
		error_exit(false, "Requires filename of configuration ini-file");

	load_ini_file(argv[1]);

	setlogfile(logfile.c_str(), LL_DEBUG);

	log(LL_INFO, "Configuration-file loaded");

	if (db_url.empty() == false)
		d = new db(db_url, db_user, db_pass);

	char txbuf[MAX_PACKET_SIZE] { 0 };

	LoRa_ctl modem;

	memset(&modem, 0x00, sizeof modem);

	//See for typedefs, enumerations and there values in LoRa.h header file
	modem.spiCS = 0;//Raspberry SPI CE pin number
	modem.rx.callback = rx_f;
	modem.tx.data.buf = txbuf;
	modem.tx.callback = tx_f;
	modem.eth.preambleLen = 8;
	modem.eth.bw = BW125;//Bandwidth 125KHz
	modem.eth.sf = SF12;//Spreading Factor 12
	modem.eth.ecr = CR5;//Error coding rate CR4/8
	modem.eth.freq = 433775000;// 434.8MHz
	modem.eth.resetGpioN = gpio_lora_reset;
	modem.eth.dio0GpioN = gpio_lora_dio0;
	modem.eth.outPower = OP20;//Output power
	modem.eth.powerOutPin = PA_BOOST;//Power Amplifire pin
	modem.eth.AGC = 1;//Auto Gain Control
	modem.eth.OCP = 240;// 45 to 240 mA. 0 to turn off protection
	modem.eth.implicitHeader = 0;//Explicit header mode
	modem.eth.syncWord = 0x12;
	modem.eth.CRC = 1;
	//For detail information about SF, Error Coding Rate, Explicit header, Bandwidth, AGC, Over current protection and other features refer to sx127x datasheet https://www.semtech.com/uploads/documents/DS_SX1276-7-8-9_W_APP_V5.pdf

	LoRa_begin(&modem);

	signal(11, SIG_DFL);

	LoRa_receive(&modem);

	int fdmaster = -1, fdslave = -1;

	if (local_ax25) {
		if (openpty(&fdmaster, &fdslave, NULL, NULL, NULL) == -1)
			error_exit(true, "openpty failed");

		int disc = N_AX25;
		if (ioctl(fdslave, TIOCSETD, &disc) == -1)
			error_exit(true, "error setting line discipline");

		if (setifcall(fdslave, callsign.c_str()) == -1)
			error_exit(false, "cannot set call");

		int v = 4;
		if (ioctl(fdslave, SIOCSIFENCAP, &v) == -1)
			error_exit(true, "failed to set encapsulation");

		char dev_name[64] = { 0 };
		if (ioctl(fdslave, SIOCGIFNAME, dev_name) == -1)
			error_exit(true, "failed retrieving name of ax25 network device name");

		startiface(dev_name);
	}

	struct mosquitto *mi = nullptr;

	int err = 0;
	if (mqtt_host.empty() == false) {
		mi = mosquitto_new(nullptr, true, nullptr);
		if (!mi)
			error_exit(false, "Cannot crate mosquitto instance");

		if ((err = mosquitto_connect(mi, mqtt_host.c_str(), mqtt_port, 30)) != MOSQ_ERR_SUCCESS)
			error_exit(false, "mqtt failed to connect (%s)", mosquitto_strerror(err));

		if ((err = mosquitto_loop_start(mi)) != MOSQ_ERR_SUCCESS)
			error_exit(false, "mqtt failed to start thread (%s)", mosquitto_strerror(err));
	}

	std::thread tx_thread([fdmaster, mi] {
		log(LL_INFO, "Starting transmit (LoRa APRS -> aprsi) thread");

		int fd = -1;

		for(;;) {
			pthread_setname_np(pthread_self(), "tx_thread");

			std::unique_lock<std::mutex> lck(packets_lock);

			while(packets.empty())
				packets_cv.wait(lck);

			rxData rx = packets.front();
			packets.pop();

			lck.unlock();

			double latitude = 0, longitude = 0, distance = -1.0;

			char *colon = strchr(rx.buf, ':');
			if (colon && rx.size - (rx.buf - colon) >= 7) {
				parse_nmea_pos(colon + 1, &latitude, &longitude);

				distance = calcGPSDistance(latitude, longitude, local_lat, local_lng);
			}

			if (d)
				d->insert_message(reinterpret_cast<uint8_t *>(rx.buf), rx.size, rx.RSSI, rx.SNR, rx.CRC, latitude, longitude, distance);

			char buffer[32] { 0 };
			ctime_r(&rx.last_time.tv_sec, buffer);

			char *temp = strchr(buffer, '\n');
			if (temp)
				*temp = 0x00;

			log(LL_INFO, "RX message @ timestamp: %s, CRC error: %d, RSSI: %d, SNR: %f (%f,%f => distance: %fm)", buffer, rx.CRC, rx.RSSI, rx.SNR, latitude, longitude, distance);

			json_t     *meta         = nullptr;
			const char *meta_str     = nullptr;
			int         meta_str_len = 0;

			if (mi && (mqtt_aprs_packet_meta.empty() == false || mqtt_ax25_packet_meta.empty() == false)) {
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

				json_object_set(meta, "data", json_string(dump_hex(reinterpret_cast<const uint8_t *>(rx.buf), rx.size).c_str()));

				meta_str     = json_dumps(meta, 0);
				meta_str_len = strlen(meta_str);
			}

			if (mi && mqtt_aprs_packet_meta.empty() == false) {
				int err = 0;
				if ((err = mosquitto_publish(mi, nullptr, mqtt_aprs_packet_meta.c_str(), meta_str_len, meta_str, 0, false)) != MOSQ_ERR_SUCCESS)
					log(LL_WARNING, "mqtt failed to publish (%s)", mosquitto_strerror(err));
			}

			const uint8_t *const data = reinterpret_cast<const uint8_t *>(rx.buf);

			if (data[0] == 0x3c) {  // OE_
				if (fd == -1 && aprs_user.empty() == false) {
					log(LL_INFO, "(re-)connecting to aprs2.net");

					fd = connect_to("rotate.aprs2.net", 14580);

					if (fd != -1) {
						std::string login = "user " + aprs_user + " pass " + aprs_pass + " vers MyAprsGw softwarevers 0.1\r\n";

						if (WRITE(fd, reinterpret_cast<const uint8_t *>(login.c_str()), login.size()) != ssize_t(login.size())) {
							close(fd);
							fd = -1;
							log(LL_WARNING, "Failed aprsi handshake (send)");
						}

						std::string reply;

						for(;;) {
							char c = 0;
							if (read(fd, &c, 1) <= 0) {
								close(fd);
								fd = -1;
								log(LL_WARNING, "Failed aprsi handshake (receive)");
								break;
							}

							if (c == 10)
								break;

							reply += c;
						}

						log(LL_DEBUG, "recv: %s", reply.c_str());
					}
					else {
						log(LL_ERR, "failed to connect: %s", strerror(errno));
					}
				}

				if (fd != -1) {
					std::string payload(reinterpret_cast<const char *>(&data[3]), rx.size - 3);
					payload += "\r\n";

					if (WRITE(fd, reinterpret_cast<const uint8_t *>(payload.c_str()), payload.size()) != ssize_t(payload.size())) {
						close(fd);
						fd = -1;
						log(LL_WARNING, "Failed to transmit APRS data to aprsi");
					}
				}

				if (mi && mqtt_aprs_packet_as_is.empty() == false) {
					int err = 0;
					if ((err = mosquitto_publish(mi, nullptr, mqtt_aprs_packet_as_is.c_str(), rx.size, data, 0, false)) != MOSQ_ERR_SUCCESS)
						log(LL_WARNING, "mqtt failed to publish (%s)", mosquitto_strerror(err));
				}
			}
			else {
				std::string to   = get_ax25_addr(&data[0]);
				std::string from = get_ax25_addr(&data[7]);

				log(LL_INFO, "Received AX.25 over LoRa: %s -> %s", from.c_str(), to.c_str());

				if (fdmaster != -1)
					send_mkiss(fdmaster, 0, data, rx.size);

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

		if (fd != -1)
			close(fd);
	});

	if (local_ax25) {
		log(LL_INFO, "Starting transmit (local AX.25 stack to LoRa)");

		struct pollfd fds[] = { { fdmaster, POLLIN, 0 } };

		for(;;) {
			if (poll(fds, 1, -1) == -1) {
				if (errno != EINTR) {
					log(LL_WARNING, "TERMINATE: (%s)", strerror(errno));
					break;
				}
			}

			if (fds[0].revents) {
				uint8_t *p = NULL;
				int plen = 0;
				if (!recv_mkiss(fdmaster, &p, &plen, true)) {
					log(LL_WARNING, "TERMINATE");
					break;
				}

				memcpy(txbuf, p, plen);
				free(p);

				log(LL_DEBUG, "transmit: %s", dump_hex(reinterpret_cast<const uint8_t *>(txbuf), plen).c_str());

				LoRa_stop_receive(&modem); //manually stoping RxCont mode

				while(LoRa_get_op_mode(&modem) != STDBY_MODE)
					usleep(101000);

				modem.tx.data.size = plen;

				LoRa_send(&modem);

				while(LoRa_get_op_mode(&modem) != STDBY_MODE)
					usleep(101000);

				log(LL_DEBUG, "Time on air data - Tsym: %f; Tpkt: %f; payloadSymbNb: %u", modem.tx.data.Tsym, modem.tx.data.Tpkt, modem.tx.data.payloadSymbNb);

				LoRa_receive(&modem);
			}
		}
	}
	else {
		for(;;)
			sleep(86400);
	}

	LoRa_end(&modem);

	log(LL_INFO, "END");
}
