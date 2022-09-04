#include <assert.h>
#include <errno.h>
#include <optional>
#include <string>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "log.h"
#include "net.h"
#include "str.h"
#include "tranceiver-aprs-si.h"


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

transmit_error_t tranceiver_aprs_si::put_message_low(const message & m)
{
	auto content = m.get_content();

	if (content.second < 5)
		return TE_hardware;

	if (content.first[0] != '<' || content.first[1] != 0xff || content.first[2] != 0x01) {  // not an APRS packet?
		log(LL_DEBUG, "tranceiver_aprs_si::put_message_low(%s): not an APRS packet %s", m.get_id_short().c_str(), dump_hex(content.first, content.second).c_str());

		return TE_ok;
	}

	stats_inc_counter(cnt_frame_aprs);

	auto rate_limit_rc = s->check(content.first, content.second);

	if (rate_limit_rc.first == false) {
		log(LL_DEBUG_VERBOSE, "tranceiver_aprs_si::put_message_low(%s): denied by rate limiter", m.get_id_short().c_str());

		stats_inc_counter(cnt_frame_aprs_rate_limited);

		return TE_ratelimiting;
	}

	std::string content_out = std::string(&reinterpret_cast<const char *>(content.first)[3], content.second - 3);  // for OE_ only

	std::size_t gt = content_out.find('>');

	if (gt != std::string::npos) {
		std::string from;
		std::string to;

		std::size_t colon = content_out.find(':', gt);

		if (colon != std::string::npos) {
			std::string to_full = content_out.substr(gt + 1, colon - gt - 1);

			std::size_t delimiter = to_full.find(',');

			if (delimiter != std::string::npos)
				to = to_full.substr(0, delimiter);
			else
				to = to_full;

			from    = content_out.substr(0, gt);

			content_out = from + ">" + to_full + ",qAO," + local_callsign + content_out.substr(colon);

			log(LL_DEBUG_VERBOSE, "tranceiver_aprs_si::put_message_low(%s): %s => %s", m.get_id_short().c_str(), from.c_str(), to.c_str());
		}
		else {
			stats_inc_counter(cnt_aprs_invalid);
		}
	}
	else {
		stats_inc_counter(cnt_aprs_invalid);
	}

	std::unique_lock<std::mutex> lck(lock);

	if (fd == -1) {
		log(LL_INFO, "(re-)connecting to aprs2.net");

		fd = connect_to("rotate.aprs2.net", 14580);

		if (fd != -1) {
			std::string login = "user " + aprs_user + " pass " + aprs_pass + " vers MyAprsGw softwarevers 0.2\r\n";

			if (WRITE(fd, reinterpret_cast<const uint8_t *>(login.c_str()), login.size()) != ssize_t(login.size())) {
				close(fd);
				fd = -1;
				log(LL_WARNING, "Failed aprsi handshake (send)");
			}
			else {
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
		}
		else {
			log(LL_ERROR, "failed to connect: %s", strerror(errno));
		}
	}

	if (fd != -1) {
		// skip first bytes (lora aprs header)
		std::string content_out = std::string(reinterpret_cast<const char *>(content.first + 3), content.second - 3);

		std::string payload     = content_out + "\r\n";

		log(LL_DEBUG, myformat("(%s) To APRS-IS: %s", m.get_id_short().c_str(), content_out.c_str()).c_str());

		if (WRITE(fd, reinterpret_cast<const uint8_t *>(payload.c_str()), payload.size()) != ssize_t(payload.size())) {
			close(fd);
			fd = -1;
			log(LL_WARNING, "Failed to transmit %s APRS data to aprsi (%s)", m.get_id_short().c_str(), strerror(errno));
		}
	}

	return fd != -1 ? TE_ok : TE_hardware;
}

tranceiver_aprs_si::tranceiver_aprs_si(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos, const std::string & aprs_user, const std::string & aprs_pass, const std::string & local_callsign, stats *const st, int device_nr) :
	tranceiver(id, s, w, pos),
	aprs_user(aprs_user),
	aprs_pass(aprs_pass),
	local_callsign(local_callsign)
{
	log(LL_INFO, "Instantiated APRS-SI (%s)", id.c_str());

        cnt_frame_aprs              = st->register_stat(myformat("%s-aprs-frames",              get_id().c_str()), myformat("1.3.6.1.2.1.4.57850.2.4.%zu.1", device_nr), snmp_integer::si_counter64);
        cnt_frame_aprs_rate_limited = st->register_stat(myformat("%s-aprs-frames-rate-limited", get_id().c_str()), myformat("1.3.6.1.2.1.4.57850.2.4.%zu.2", device_nr), snmp_integer::si_counter64);
        cnt_aprs_invalid            = st->register_stat(myformat("%s-aprs-frames-invalid",      get_id().c_str()), myformat("1.3.6.1.2.1.4.57850.2.4.%zu.3", device_nr), snmp_integer::si_counter64);
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

tranceiver *tranceiver_aprs_si::instantiate(const libconfig::Setting & node_in, work_queue_t *const w, const position_t & pos, stats *const st, int device_nr)
{
	std::string  id;
	seen        *s = nullptr;
	std::string  aprs_user;
	std::string  aprs_pass;
	std::string  local_callsign;

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "aprs-user")
			aprs_user = node_in.lookup(type).c_str();
		else if (type == "aprs-pass")
			aprs_pass = node_in.lookup(type).c_str();
		else if (type == "repetition-rate-limiting") {
			assert(s == nullptr);
			s = seen::instantiate(node);
		}
		else if (type == "local-callsign")
			local_callsign = node_in.lookup(type).c_str();
		else if (type != "type") {
			error_exit(false, "setting \"%s\" is not known", type.c_str());
		}
        }

	if (aprs_user.empty())
		error_exit(false, "No aprs-user selected");

	if (local_callsign.empty())
		error_exit(false, "No local callsign selected");

	return new tranceiver_aprs_si(id, s, w, pos, aprs_user, aprs_pass, local_callsign, st, device_nr);
}
