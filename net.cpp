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

void transmit_udp(const std::string & host, const int portnr, const uint8_t *const data, const size_t data_len)
{
        struct addrinfo hints = { 0 };
        hints.ai_family = AF_UNSPEC;    // Allow IPv4 or IPv6
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;    // For wildcard IP address
        hints.ai_protocol = 0;          // Any protocol
        hints.ai_canonname = nullptr;
        hints.ai_addr = nullptr;
        hints.ai_next = nullptr;

        char portnr_str[8] = { 0 };
        snprintf(portnr_str, sizeof portnr_str, "%d", portnr);

        struct addrinfo *result = nullptr;
        int rc = getaddrinfo(host.c_str(), portnr_str, &hints, &result);
        if (rc != 0)
                error_exit(false, "Problem resolving %s: %s\n", host, gai_strerror(rc));

        for(struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
                int fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (fd == -1)
                        continue;

		sendto(fd, data, data_len, 0, rp->ai_addr, rp->ai_addrlen);

		close(fd);

		break;
        }

        freeaddrinfo(result);
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
