#include <errno.h>
#include <string>
#include <string.h>
#include <unistd.h>

#include "aprs-si.h"
#include "log.h"
#include "net.h"
#include "utils.h"

aprs_si::aprs_si(const std::string & aprs_user, const std::string & aprs_pass) :
	aprs_user(aprs_user), aprs_pass(aprs_pass)
{
}

aprs_si::~aprs_si()
{
	if (fd != -1)
		close(fd);
}

std::string aprs_si::receive_string(const int fd)
{
	std::string reply;

	for(;;) {
		char c = 0;
		if (read(fd, &c, 1) <= 0) {
			log(LL_WARNING, "APRS-SI: Receive failed: %s", strerror(errno));
			return "";
		}

		if (c == 10)
			break;

		reply += c;
	}

	return reply;
}

bool aprs_si::send_through_aprs_is(const std::string & content_out)
{
	aprs_is_lock.lock();

	if (fd == -1) {
		log(LL_INFO, "(re-)connecting to aprs2.net");

		fd = connect_to("rotate.aprs2.net", 14580);

		if (fd != -1) {
			std::string login = "user " + aprs_user + " pass " + aprs_pass + " vers MyAprsGw softwarevers 0.1\r\n";

			if (WRITE(fd, reinterpret_cast<const uint8_t *>(login.c_str()), login.size()) != ssize_t(login.size())) {
				close(fd);
				fd = -1;
				log(LL_WARNING, "Failed aprsi handshake (send)");
			}

			std::string reply = receive_string(fd);

			if (reply.empty()) {
				log(LL_WARNING, "Failed aprsi handshake (receive)");
				close(fd);
				fd = -1;
			}
			else {
				log(LL_DEBUG, "recv: %s", reply.c_str());
			}
		}
		else {
			log(LL_ERR, "failed to connect: %s", strerror(errno));
		}
	}

	if (fd != -1) {
		std::string payload = content_out + "\r\n";

		log(LL_DEBUG, myformat("To APRS-IS: %s", content_out.c_str()).c_str());

		if (WRITE(fd, reinterpret_cast<const uint8_t *>(payload.c_str()), payload.size()) != ssize_t(payload.size())) {
			close(fd);
			fd = -1;
			log(LL_WARNING, "Failed to transmit APRS data to aprsi (%s)", strerror(errno));
		}
	}

	bool ok = fd >= 0;

	aprs_is_lock.unlock();

	return ok;
}
