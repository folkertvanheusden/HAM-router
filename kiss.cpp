#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "log.h"
#include "net.h"
#include "utils.h"

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

		if (rc == -1 || rc == 0) {
			if (rc == -1 && errno == EINTR)
				continue;

			error_exit(true, "failed writing to mkiss device");
		}

		tmp += rc;
		out_len -= rc;
	}

	free(out);
}

