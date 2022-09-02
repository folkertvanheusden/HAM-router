#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netax25/axlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "error.h"
#include "log.h"
#include "net.h"

void set_nodelay(int fd)
{
        int on = 1;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &on, sizeof(int)) == -1)
                error_exit(true, "TCP_NODELAY");
}

int WRITE(int fd, const uint8_t *whereto, size_t len)
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
                int fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (fd == -1)
                        continue;

                if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
                        freeaddrinfo(result);

			set_nodelay(fd);

                        return fd;
                }

                close(fd);
        }

        freeaddrinfo(result);

        return -1;
}

bool transmit_udp(const std::string & dest, const uint8_t *const data, const size_t data_len)
{
        struct addrinfo hints { 0 };
        hints.ai_family    = AF_UNSPEC;    // Allow IPv4 or IPv6
        hints.ai_socktype  = SOCK_DGRAM;
        hints.ai_flags     = AI_PASSIVE;    // For wildcard IP address
        hints.ai_protocol  = 0;          // Any protocol
        hints.ai_canonname = nullptr;
        hints.ai_addr      = nullptr;
        hints.ai_next      = nullptr;

	std::size_t colon = dest.find(":");
	if (colon == std::string::npos) {
                log(LL_ERROR, "Port number missing (%s)", dest.c_str());

		return false;
	}

	std::string portnr = dest.substr(colon + 1);

	std::string host   = dest.substr(0, colon);

        struct addrinfo *result = nullptr;
        int rc = getaddrinfo(host.c_str(), portnr.c_str(), &hints, &result);
        if (rc != 0) {
                log(LL_WARNING, "Problem resolving %s: %s", host.c_str(), gai_strerror(rc));

		return false;
	}

	bool ok = true;

        for(struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
                int fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (fd == -1)
                        continue;

		if (sendto(fd, data, data_len, 0, rp->ai_addr, rp->ai_addrlen) != ssize_t(data_len))
			ok = false;

		close(fd);

		break;
        }

        freeaddrinfo(result);

	return ok;
}

void startiface(const char *dev)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1)
		error_exit(true, "Cannot create (dummy) socket");

	struct ifreq ifr { 0 };

	if (strlen(dev) >= sizeof ifr.ifr_name)
		error_exit(false, "Network device name too long");

	strncpy(ifr.ifr_name, dev, sizeof ifr.ifr_name);
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

std::string get_ax25_addr(const uint8_t *const in)
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

uint16_t get_net_short(const uint8_t *const p)
{
	return (p[0] << 8) | p[1];
}

uint32_t get_net_long(const uint8_t *const p)
{
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

uint64_t get_net_long_long(const uint8_t *const p)
{
	uint64_t out = 0;

	for(int i=0; i<8; i++) {
		out <<= 8;
		out |= p[i];
	}

	return out;
}

void put_net_long(uint8_t *const p, const uint32_t v)
{
	p[0] = v >> 24;
	p[1] = v >> 16;
	p[2] = v >>  8;
	p[3] = v;
}

void put_net_long_long(uint8_t *const p, const uint64_t v)
{
	p[0] = v >> 56;
	p[1] = v >> 48;
	p[2] = v >> 40;
	p[3] = v >> 32;
	p[4] = v >> 24;
	p[5] = v >> 16;
	p[6] = v >>  8;
	p[7] = v;
}
