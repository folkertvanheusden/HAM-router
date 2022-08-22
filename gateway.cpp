#include <condition_variable>
#include <errno.h>
#include <math.h>
#include <mutex>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <pty.h>
#include <queue>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netax25/axlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

extern "C" {
#include "LoRa.h"
}

#include "error.h"
#include "log.h"
#include "db.h"

constexpr char callsign[] = "PD9FVH";
const std::string aprs_user = "PD9FVH";
const std::string aprs_pass = "19624";
constexpr double local_lat = 52.0275;
constexpr double local_lng = 4.6955;
const std::string db_url = "tcp://192.168.64.1/lora-aprs";
const std::string db_user = "lora";
const std::string db_pass = "mauw";

db *d = nullptr;

#define MAX_PACKET_SIZE 254

void set_nodelay(int fd)
{
        int on = 1;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &on, sizeof(int)) == -1)
                error_exit(true, "TCP_NODELAY");
}

ssize_t WRITE(int fd, const uint8_t *whereto, size_t len)
{
        ssize_t cnt=0;

        while(len > 0)
        {
                ssize_t rc = write(fd, whereto, len);
                if (rc <= 0)
                        return rc;

                whereto += rc;
                len -= rc;
                cnt += rc;
        }

        return cnt;
}

int connect_to(const char *host, const int portnr)
{
        struct addrinfo hints = { 0 };
        hints.ai_family = AF_UNSPEC;    // Allow IPv4 or IPv6
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;    // For wildcard IP address
        hints.ai_protocol = 0;          // Any protocol
        hints.ai_canonname = nullptr;
        hints.ai_addr = nullptr;
        hints.ai_next = nullptr;

        char portnr_str[8] = { 0 };
        snprintf(portnr_str, sizeof portnr_str, "%d", portnr);

        struct addrinfo *result = nullptr;
        int rc = getaddrinfo(host, portnr_str, &hints, &result);
        if (rc != 0)
                error_exit(false, "Problem resolving %s: %s\n", host, gai_strerror(rc));

        for(struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
                int fd = socket(rp -> ai_family, rp -> ai_socktype, rp -> ai_protocol);
                if (fd == -1)
                        continue;

                if (connect(fd, rp -> ai_addr, rp -> ai_addrlen) == 0) {
                        freeaddrinfo(result);

			set_nodelay(fd);

                        return fd;
                }

                close(fd);
        }

        freeaddrinfo(result);

        return -1;
}

void startiface(const char *dev)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1)
		error_exit(true, "Cannot create (dummy) socket");

	struct ifreq ifr;
	strcpy(ifr.ifr_name, dev);
	ifr.ifr_mtu = MAX_PACKET_SIZE;

	if (ioctl(fd, SIOCSIFMTU, &ifr) == -1)
		error_exit(true, "failed setting mtu size for ax25 device");

        if (ioctl(fd, SIOCGIFFLAGS, &ifr) == -1)
		error_exit(true, "failed retrieving current ax25 device settings");

	ifr.ifr_flags &= IFF_NOARP;
	ifr.ifr_flags |= IFF_UP;
	ifr.ifr_flags |= IFF_RUNNING;
//	ifr.ifr_flags |= IFF_BROADCAST;

	if (ioctl(fd, SIOCSIFFLAGS, &ifr) == -1)
		error_exit(true, "failed setting ax25 device settings");

	close(fd);
}

void dump_hex(const unsigned char *p, int len)
{
	for(int i=0; i<len; i++)
		printf("%d[%c] ", p[i], p[i] > 32 && p[i] < 127 ? p[i] : '.');
}

#define FEND	0xc0
#define FESC	0xdb
#define TFEND	0xdc
#define TFESC	0xdd

bool recv_mkiss(int fd, unsigned char **p, int *len, bool verbose)
{
	bool first = true, ok = false, escape = false;

	*p = (unsigned char *)malloc(MAX_PACKET_SIZE);
	*len = 0;

	for(;;)
	{
		unsigned char buffer = 0;

		if (read(fd, &buffer, 1) == -1) {
			if (errno == EINTR)
				continue;

			error_exit(true, "failed reading from mkiss device");
		}

		if (escape)
		{
			if (*len == MAX_PACKET_SIZE)
				break;

			if (buffer == TFEND)
				(*p)[(*len)++] = FEND;
			else if (buffer == TFESC)
				(*p)[(*len)++] = FESC;
			else if (verbose)
				printf("unexpected mkiss escape %02x\n", buffer);

			escape = false;
		}
		else if (buffer == FEND)
		{
			if (first)
				first = false;
			else
			{
				ok = true;
				break;
			}
		}
		else if (buffer == FESC)
			escape = true;
		else
		{
			if (*len == MAX_PACKET_SIZE)
				break;

			(*p)[(*len)++] = buffer;
		}
	}

	if (ok)
	{
		int cmd = (*p)[0] & 0x0f;

		if (verbose)
			printf("port: %d\n", ((*p)[0] >> 4) & 0x0f);

		if (cmd == 0x00) // data frame
		{
			(*len)--;
			memcpy(&(*p)[0], &(*p)[1], *len);
		}
		else
		{
			if (verbose)
			{
				if (cmd == 1)
					printf("TX delay: %d\n", (*p)[1] * 10);
				else if (cmd == 2)
					printf("persistance: %d\n", (*p)[1] * 256 - 1);
				else if (cmd == 3)
					printf("slot time: %dms\n", (*p)[1] * 10);
				else if (cmd == 4)
					printf("txtail: %dms\n", (*p)[1] * 10);
				else if (cmd == 5)
					printf("full duplex: %d\n", (*p)[1]);
				else if (cmd == 6)
				{
					printf("set hardware: ");
					dump_hex(&(*p)[1], *len - 1);
				}
				else if (cmd == 15)
				{
					error_exit(false, "kernal asked for shutdown");
				}
			}

			ok = false; // it is ok, we just ignore it
		}
	}

	return ok;
}

void send_mkiss(int fd, int channel, const unsigned char *p, const int len)
{
	int max_len = len * 2 + 1;
	unsigned char *out = (unsigned char *)malloc(max_len);
	int offset = 0;

	out[offset++] = FEND;
	out[offset++] = (channel << 4) | 0x00; // [channel 0][data]

	for(int i=0; i<len; i++)
	{
		if (p[i] == FEND)
		{
			out[offset++] = FESC;
			out[offset++] = TFEND;
		}
		else if (p[i] == FESC)
		{
			out[offset++] = FESC;
			out[offset++] = TFESC;
		}
		else
		{
			out[offset++] = p[i];
		}
	}

	out[offset++] = FEND;

	const unsigned char *tmp = out;
	int out_len = offset;
	while(out_len)
	{
		int rc = write(fd, tmp, out_len);

		if (rc == -1 || rc == 0)
			error_exit(true, "failed writing to mkiss device");

		tmp += rc;
		out_len -= rc;
	}

	free(out);
}

class packet
{
private:
	uint8_t *data;
	int      size;

public:
	packet(const uint8_t *const in, const int in_size) {
		data = (uint8_t *)calloc(in_size + 1, 1);

		memcpy(data, in, in_size);

		size = in_size;
	}

	~packet() {
		free(data);
	}

	const uint8_t *get_data() const {
		return data;
	}

	int get_size() const {
		return size;
	}
};

std::queue<packet *>    packets;
std::mutex              packets_lock;
std::condition_variable packets_cv;
unsigned                count { 0 };

void tx_f(txData *tx)
{
	printf("transmitted\n");
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

void rx_f(rxData *rx)
{
	if (rx->size == 0 || rx->CRC)
		return;

	packet *p = new packet(reinterpret_cast<uint8_t *>(rx->buf), rx->size);

	packets_lock.lock();
	packets.push(p);
	packets_cv.notify_one();
	unsigned current_count = ++count;
	packets_lock.unlock();

	double latitude = 0, longitude = 0, distance = -1.0;

	char *colon = strchr(rx->buf, ':');
	if (colon && rx->size - (rx->buf - colon) >= 7) {
		parse_nmea_pos(colon + 1, &latitude, &longitude);

		distance = calcGPSDistance(latitude, longitude, local_lat, local_lng);
	}

	d->insert_message(reinterpret_cast<uint8_t *>(rx->buf), rx->size, rx->RSSI, rx->SNR, rx->CRC, latitude, longitude, distance);

	time_t now = time(NULL);

	char *buffer = ctime(&now);
	char *temp = strchr(buffer, '\n');
	if (temp)
		*temp = 0x00;

	printf("RX finished of %dth message @ timestamp: %s, CRC error: %d, RSSI: %d, SNR: %f (%f,%f => distance: %fm)\n", current_count, buffer, rx->CRC, rx->RSSI, rx->SNR, latitude, longitude, distance);
}

std::string get_addr(const uint8_t *const in)
{
	std::string out;

	for(int i=0; i<6; i++) {
		char c = char(in[i] >> 1);

		if (c == 32)
			continue;

		out += c;
	}

	char c = char(in[6] >> 1);

	if (c != 32) {
		out += "-";
		out += c;
	}

	return out;
}

int setifcall(int fd, const char *name)
{
        char call[7] = { 0 };

        if (ax25_aton_entry(name, call) == -1)
                return -1;

        if (ioctl(fd, SIOCSIFHWADDR, call) == -1)
		error_exit(true, "ioctl(SIOCSIFHWADDR) failed");
 
        return 0;
}

int main(int argc, char *argv[])
{
	setlogfile("gateway.log", LL_DEBUG);

	d = new db(db_url, db_user, db_pass);

	char rxbuf[255] { 0 };
	char txbuf[MAX_PACKET_SIZE] { 0 };

	LoRa_ctl modem;

	memset(&modem, 0x00, sizeof modem);

	//See for typedefs, enumerations and there values in LoRa.h header file
	modem.spiCS = 0;//Raspberry SPI CE pin number
	modem.rx.callback = rx_f;
	modem.rx.data.buf = rxbuf;
	modem.tx.data.buf = txbuf;
	modem.tx.callback = tx_f;
	modem.eth.preambleLen = 8;
	modem.eth.bw = BW125;//Bandwidth 125KHz
	modem.eth.sf = SF12;//Spreading Factor 12
	modem.eth.ecr = CR5;//Error coding rate CR4/8
	modem.eth.freq = 433775000;// 434.8MHz
	modem.eth.resetGpioN = 27;//GPIO4 on lora RESET pi
	modem.eth.dio0GpioN = 17;//GPIO17 on lora DIO0 pin to control Rxdone and Txdone interrupts
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
	if (openpty(&fdmaster, &fdslave, NULL, NULL, NULL) == -1)
		error_exit(true, "openpty failed");

	int disc = N_AX25;
	if (ioctl(fdslave, TIOCSETD, &disc) == -1)
		error_exit(true, "error setting line discipline");

	if (setifcall(fdslave, callsign) == -1)
		error_exit(false, "cannot set call");

	int v = 4;
	if (ioctl(fdslave, SIOCSIFENCAP, &v) == -1)
		error_exit(true, "failed to set encapsulation");

	char dev_name[64] = { 0 };
	if (ioctl(fdslave, SIOCGIFNAME, dev_name) == -1)
		error_exit(true, "failed retrieving name of ax25 network device name");

	startiface(dev_name);

	std::thread tx_thread([fdmaster] {
		int fd = -1;

		for(;;) {
			pthread_setname_np(pthread_self(), "tx_thread");

			std::unique_lock<std::mutex> lck(packets_lock);

			while(packets.empty())
				packets_cv.wait(lck);

			packet *p = packets.front();
			packets.pop();

			lck.unlock();

			const uint8_t *const data = p->get_data();

			if (data[0] == 0x3c) {  // OE_
				printf("LoRa: %s\n", data);

				if (fd == -1) {
					printf("(re-)connecting to aprs2.net\n");

					fd = connect_to("rotate.aprs2.net", 14580);

					if (fd != -1) {
						std::string login = "user " + aprs_user + " pass " + aprs_pass + " vers MyAprsGw softwarevers 0.1\r\n";

						if (WRITE(fd, reinterpret_cast<const uint8_t *>(login.c_str()), login.size()) != ssize_t(login.size())) {
							close(fd);
							fd = -1;
						}

						std::string reply;

						for(;;) {
							char c = 0;
							if (read(fd, &c, 1) <= 0) {
								printf("failed receive\n");
								close(fd);
								fd = -1;
								break;
							}

							if (c == 10)
								break;

							reply += c;
						}

						printf("recv: %s\n", reply.c_str());
					}
					else {
						printf("failed to connect: %s\n", strerror(errno));
					}
				}

				if (fd != -1) {
					std::string payload(reinterpret_cast<const char *>(&data[3]), p->get_size() - 3);
					payload += "\r\n";

					if (WRITE(fd, reinterpret_cast<const uint8_t *>(payload.c_str()), payload.size()) != ssize_t(payload.size())) {
						close(fd);
						fd = -1;
						printf("failed to send (1)\n");
					}
				}
			}
			else {
				std::string to   = get_addr(&data[0]);
				std::string from = get_addr(&data[7]);

				printf("LoRa: %s -> %s\n", from.c_str(), to.c_str());

				send_mkiss(fdmaster, 0, data, p->get_size());
			}

			delete p;
		}

		if (fd != -1)
			close(fd);
	});

	struct pollfd fds[] = { { fdmaster, POLLIN, 0 } };

	pthread_setname_np(pthread_self(), "rx_thread");

	for(;;) {
		if (poll(fds, 1, 0) == -1) {
			printf("TERMINATE: (%s)\n", strerror(errno));
			break;
		}

		uint8_t *p = NULL;
		int plen = 0;
		if (!recv_mkiss(fdmaster, &p, &plen, true)) {
			printf("TERMINATE\n");
			break;
		}

		memcpy(txbuf, p, plen);
		free(p);

		time_t now = time(NULL);
		char *buffer = ctime(&now);
		char *temp = strchr(buffer, '\n');
		if (temp)
			*temp = 0x00;

		printf("%s: transmit ", buffer);
		dump_hex(reinterpret_cast<const uint8_t *>(txbuf), plen);
		printf("\n");

		LoRa_stop_receive(&modem); //manually stoping RxCont mode

		while(LoRa_get_op_mode(&modem) != STDBY_MODE)
			usleep(101000);

		modem.tx.data.size = plen;

		LoRa_send(&modem);

		while(LoRa_get_op_mode(&modem) != STDBY_MODE)
			usleep(101000);

		printf("Time on air data - Tsym: %f;\t", modem.tx.data.Tsym);
		printf("Tpkt: %f;\t", modem.tx.data.Tpkt);
		printf("payloadSymbNb: %u\n", modem.tx.data.payloadSymbNb);

		LoRa_receive(&modem);
	}

	LoRa_end(&modem);

	printf("end\n");
}
