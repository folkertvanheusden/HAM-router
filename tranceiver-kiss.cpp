#include <errno.h>
#include <optional>
#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "log.h"
#include "net.h"
#include "tranceiver-kiss.h"
#include "utils.h"


#define FEND	0xc0
#define FESC	0xdb
#define TFEND	0xdc
#define TFESC	0xdd

#define MAX_PACKET_LEN 256  // TODO: make dynamic size

bool tranceiver_kiss::recv_mkiss(unsigned char **p, int *len, bool verbose)
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

			log(LL_ERR, "failed reading from mkiss device");

			return false;
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
				log(LL_ERR, "unexpected mkiss escape %02x", buffer);

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
			log(LL_DEBUG, "port: %d", ((*p)[0] >> 4) & 0x0f);

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
					log(LL_DEBUG, "TX delay: %d\n", (*p)[1] * 10);
				else if (cmd == 2)
					log(LL_DEBUG, "persistance: %d\n", (*p)[1] * 256 - 1);
				else if (cmd == 3)
					log(LL_DEBUG, "slot time: %dms\n", (*p)[1] * 10);
				else if (cmd == 4)
					log(LL_DEBUG, "txtail: %dms\n", (*p)[1] * 10);
				else if (cmd == 5)
					log(LL_DEBUG, "full duplex: %d\n", (*p)[1]);
				else if (cmd == 6) {
					log(LL_DEBUG, "set hardware: %s", dump_hex(&(*p)[1], *len - 1));
				}
				else if (cmd == 15) {
					error_exit(false, "kernel asked for shutdown");
				}
			}

			ok = false; // it is ok, we just ignore it
		}
	}

	return ok;
}

bool tranceiver_kiss::send_mkiss(int channel, const unsigned char *p, const int len)
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

		if (rc == -1 || rc == 0) {
			if (rc == -1 && errno == EINTR)
				continue;

			log(LL_ERR, "failed writing to mkiss device");

			return false;
		}

		tmp += rc;
		out_len -= rc;
	}

	free(out);

	return true;
}

transmit_error_t tranceiver_kiss::put_message_low(const uint8_t *const p, const size_t len)
{
	if (send_mkiss(0, p, len))
		return TE_ok;

	return TE_hardware;
}

tranceiver_kiss::tranceiver_kiss(const std::string & id, seen *const s, const std::string & callsign, const std::string & if_up) :
	tranceiver(id, s)
{
	int fd_master = -1;
	int fd_slave  = -1;

	log(LL_INFO, "Configuring kiss interface");

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

	fd = fd_master;
}

tranceiver_kiss::~tranceiver_kiss()
{
}

void tranceiver_kiss::operator()()
{
	pollfd fds[] = { { fd, POLLIN, 0 } };

	while(!terminate) {
		int rc = poll(fds, 1, 100);

		if (rc == 0)
			continue;

		if (rc == -1)
			break;

		uint8_t *p   = nullptr;
		int      len = 0;
		if (!recv_mkiss(&p, &len, true))
			break;

		message_t m { 0 };
		gettimeofday(&m.tv, nullptr);
		m.message = p;
		m.s       = len;

		queue_incoming_message(m);
	}
}

tranceiver *tranceiver_kiss::instantiate(const libconfig::Setting & node_in)
{
	std::string  id;
	seen        *s = nullptr;
	std::string  callsign;
	std::string  if_up;

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "incoming-rate-limiting")
			s = seen::instantiate(node);
		else if (type == "callsign")
			callsign = node_in.lookup(type).c_str();
		else if (type == "if-up")
			if_up = node_in.lookup(type).c_str();
		else
			error_exit(false, "setting \"%s\" is now known", type.c_str());
        }

	if (callsign.empty())
		error_exit(false, "No callsign selected");

	return new tranceiver_kiss(id, s, callsign, if_up);
}
