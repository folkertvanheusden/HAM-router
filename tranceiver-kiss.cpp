#include <assert.h>
#include <errno.h>
#include <optional>
#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "error.h"
#include "log.h"
#include "net.h"
#include "random.h"
#include "str.h"
#include "time.h"
#include "tranceiver-kiss.h"
#include "utils.h"


#define FEND	0xc0
#define FESC	0xdb
#define TFEND	0xdc
#define TFESC	0xdd

#define MAX_PACKET_LEN 256  // TODO: make dynamic size

bool tranceiver_kiss::recv_mkiss(uint8_t **const p, int *const len)
{
	bool ok     = false;
	bool escape = false;

	*p   = reinterpret_cast<uint8_t *>(malloc(MAX_PACKET_SIZE));
	*len = 0;

	for(;!terminate;)
	{
		uint8_t buffer = 0;

		if (read(fd, &buffer, 1) == -1) {
			if (errno == EINTR)
				continue;

			log(LL_ERROR, "kiss(%s): failed reading from device", get_id().c_str());

			free(*p);

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
			else
				log(LL_WARNING, "kiss(%s) unexpected escape %02x", get_id().c_str(), buffer);

			escape = false;
		}
		else if (buffer == FEND)
		{
			if (*len) {
				ok = true;
				break;
			}

			// otherwise: first FEND, ignore
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

	if (*len == 0)
		ok = false;

	if (ok)
	{
		int cmd = (*p)[0] & 0x0f;

		log(LL_DEBUG, "kiss(%s): port: %d, cmd: %d, len: %d", get_id().c_str(), ((*p)[0] >> 4) & 0x0f, cmd, *len);

		(*len)--;

		if (*len)
			memcpy(&(*p)[0], &(*p)[1], *len);

		if (cmd == 1)
			log(LL_DEBUG, "kiss(%s): TX delay: %d\n", get_id().c_str(), (*p)[1] * 10);
		else if (cmd == 2)
			log(LL_DEBUG, "kiss(%s): persistance: %d\n", get_id().c_str(), (*p)[1] * 256 - 1);
		else if (cmd == 3)
			log(LL_DEBUG, "kiss(%s): slot time: %dms\n", get_id().c_str(), (*p)[1] * 10);
		else if (cmd == 4)
			log(LL_DEBUG, "kiss(%s): txtail: %dms\n", get_id().c_str(), (*p)[1] * 10);
		else if (cmd == 5)
			log(LL_DEBUG, "kiss(%s): full duplex: %d\n", get_id().c_str(), (*p)[1]);
		else if (cmd == 6)
			log(LL_DEBUG, "kiss(%s): set hardware: %s", get_id().c_str(), dump_hex(&(*p)[1], *len - 1).c_str());
		else if (cmd == 15)
			log(LL_INFO, "kiss(%s): kernel asked for shutdown", get_id().c_str());
	}

	if (!ok)
		free(*p);

	return ok;
}

void escape_put(uint8_t **p, int *len, uint8_t c)
{
	if (c == FEND) {
		(*p)[(*len)++] = FESC;
		(*p)[(*len)++] = TFEND;
	}
	else if (c == FESC) {
		(*p)[(*len)++] = FESC;
		(*p)[(*len)++] = TFESC;
	}
	else {
		(*p)[(*len)++] = c;
	}
}

bool tranceiver_kiss::send_mkiss(const uint8_t cmd, const uint8_t channel, const uint8_t *const p, const int len)
{
	int      max_len = len * 2 + 1;
	uint8_t *out     = reinterpret_cast<uint8_t *>(malloc(max_len));
	int      offset  = 0;

	assert(cmd < 16);
	assert(channel < 16);

	out[offset++] = FEND;

	escape_put(&out, &offset, (channel << 4) | cmd);

	for(int i=0; i<len; i++)
		escape_put(&out, &offset, p[i]);

	out[offset++] = FEND;

	if (WRITE(fd, out, offset) != offset) {
		log(LL_ERROR, "kiss(%s): failed writing to mkiss device", get_id().c_str());

		free(out);

		return false;
	}

	free(out);

	return true;
}

transmit_error_t tranceiver_kiss::put_message_low(const message & m)
{
	log(LL_DEBUG, "kiss(%s): send", m.get_id_short().c_str());

	std::unique_lock<std::mutex> lck(lock);

	auto content = m.get_content();

	if (send_mkiss(0, 0, content.first, content.second))
		return TE_ok;

	return TE_hardware;
}

tranceiver_kiss::tranceiver_kiss(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos) :
	tranceiver(id, s, w, pos)
{
}

tranceiver_kiss::~tranceiver_kiss()
{
}

void tranceiver_kiss::operator()()
{
	set_thread_name("t-kiss");

	log(LL_INFO, "kiss(%s): started thread", get_id().c_str());

	pollfd fds[] = { { fd, POLLIN, 0 } };

	while(!terminate) {
		int rc = poll(fds, 1, END_CHECK_INTERVAL_ms);

		if (rc == 0)
			continue;

		if (rc == -1)
			break;

		uint8_t *p   = nullptr;
		int      len = 0;
		if (!recv_mkiss(&p, &len))
			continue;

		timeval tv;
		gettimeofday(&tv, nullptr);

		message m(tv,
				myformat("kiss(%s)", get_id().c_str()),
				get_random_uint64_t(),
				p,
				len);

		log(LL_DEBUG_VERBOSE, "kiss(%s): received message (%s)", get_id().c_str(), dump_hex(p, len).c_str());

		free(p);

		queue_incoming_message(m);
	}
}
