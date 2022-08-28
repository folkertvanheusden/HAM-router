#include <errno.h>
#include <optional>
#include <string>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "log.h"
#include "net.h"
#include "tranceiver-aprs-si.h"
#include "utils.h"


static std::optional<std::string> receive_string(const int fd)
{
	std::string reply;

	for(;;) {
		char c = 0;
		if (read(fd, &c, 1) <= 0) {
			log(LL_WARNING, "APRS-SI: Receive failed: %s", strerror(errno));
			return { };
		}

		if (c == 10)
			break;

		reply += c;
	}

	return reply;
}

transmit_error_t tranceiver_aprs_si::put_message_low(const uint8_t *const p, const size_t size)
{
	if (s->check(p, size) == false)
		return TE_ratelimiting;

	std::unique_lock<std::mutex> lck(lock);

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

			std::optional<std::string> reply = receive_string(fd);

			if (reply.has_value() == false) {
				log(LL_WARNING, "Failed aprsi handshake (receive)");
				close(fd);
				fd = -1;
			}
			else {
				log(LL_DEBUG, "recv: %s", reply.value().c_str());
			}
		}
		else {
			log(LL_ERR, "failed to connect: %s", strerror(errno));
		}
	}

	if (fd != -1) {
		std::string content_out = std::string(reinterpret_cast<const char *>(p), size);

		std::string payload     = content_out + "\r\n";

		log(LL_DEBUG, myformat("To APRS-IS: %s", content_out.c_str()).c_str());

		if (WRITE(fd, reinterpret_cast<const uint8_t *>(payload.c_str()), payload.size()) != ssize_t(payload.size())) {
			close(fd);
			fd = -1;
			log(LL_WARNING, "Failed to transmit APRS data to aprsi (%s)", strerror(errno));
		}
	}

	return fd != -1 ? TE_ok : TE_hardware;
}

tranceiver_aprs_si::tranceiver_aprs_si(const std::string & id, seen *const s, const std::string & aprs_user, const std::string & aprs_pass) :
	tranceiver(id, s),
	aprs_user(aprs_user),
	aprs_pass(aprs_pass)
{
}

tranceiver_aprs_si::~tranceiver_aprs_si()
{
	if (fd != -1)
		close(fd);
}

void tranceiver_aprs_si::operator()()
{
	// no-op
}

tranceiver *tranceiver_aprs_si::instantiate(const libconfig::Setting & node_in)
{
	std::string  id;
	seen        *s = nullptr;
	std::string  aprs_user;
	std::string  aprs_pass;

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "aprs-user")
			aprs_user = node_in.lookup(type).c_str();
		else if (type == "aprs-pass")
			aprs_pass = node_in.lookup(type).c_str();
		else
			error_exit(false, "setting \"%s\" is now known", type.c_str());
        }

	if (aprs_user.empty())
		error_exit(false, "No aprs-user selected");

	return new tranceiver_aprs_si(id, s, aprs_user, aprs_pass);
}
