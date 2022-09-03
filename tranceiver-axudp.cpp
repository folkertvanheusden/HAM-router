#include <assert.h>
#include <errno.h>
#include <optional>
#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "crc_ppp.h"
#include "error.h"
#include "log.h"
#include "net.h"
#include "random.h"
#include "str.h"
#include "time.h"
#include "tranceiver-axudp.h"
#include "utils.h"


transmit_error_t tranceiver_axudp::put_message_low(const message & m)
{
	auto     content  = m.get_content();
	size_t   len      = content.second;
	size_t   temp_len = len + 2;
	uint8_t *temp     = reinterpret_cast<uint8_t *>(malloc(temp_len));

	memcpy(temp, content.first, len);

	uint16_t crc = compute_crc(const_cast<uint8_t *>(content.first), len);

	temp[len] = crc;
	temp[len + 1] = crc >> 8;

	for(auto d : destinations) {
		log(LL_DEBUG_VERBOSE, "tranceiver_axudp::put_message_low(%s): transmit to %s (%s)", m.get_id_short().c_str(), d.c_str(), dump_replace(temp, temp_len).c_str());

		if (transmit_udp(d, temp, temp_len) == false && continue_on_error == false) {
			log(LL_WARNING, "Problem sending %s", m.get_id_short().c_str());

			free(temp);

			return TE_hardware;
		}
	}

	free(temp);

	return TE_ok;
}

tranceiver_axudp::tranceiver_axudp(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos, const int listen_port, const std::vector<std::string> & destinations, const bool continue_on_error, const bool distribute) :
	tranceiver(id, s, w, pos),
	listen_port(listen_port),
	destinations(destinations),
	continue_on_error(continue_on_error),
	distribute(distribute)
{
	log(LL_INFO, "Instantiated AXUDP (%s)", id.c_str());

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

        struct sockaddr_in servaddr { 0 };

        servaddr.sin_family      = AF_INET; // IPv4
        servaddr.sin_addr.s_addr = INADDR_ANY;
        servaddr.sin_port        = htons(listen_port);

        if (bind(fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
                error_exit(true, "bind to port %d failed", listen_port);

	th = new std::thread(std::ref(*this));
}

tranceiver_axudp::~tranceiver_axudp()
{
	close(fd);

	terminate = true;

	th->join();
	delete th;
}

transmit_error_t tranceiver_axudp::send_to_other_axudp_targets(const message & m, const std::string & came_from)
{
	for(auto d : destinations) {
		if (d == came_from) {
			log(LL_DEBUG_VERBOSE, "tranceiver_axudp::send_to_other_axudp_targets(%s): not (re-)sending to %s", m.get_id_short().c_str(), d.c_str());

			continue;
		}

		log(LL_DEBUG_VERBOSE, "tranceiver_axudp::send_to_other_axudp_targets(%s): transmit to %s", m.get_id_short().c_str(), d.c_str());

		auto content = m.get_content();

		if (transmit_udp(d, content.first, content.second) == false && continue_on_error == false) {
			log(LL_WARNING, "Problem sending %s", m.get_id_short().c_str());

			return TE_hardware;
		}
	}

	return TE_ok;
}

void tranceiver_axudp::operator()()
{
	set_thread_name("t-axudp");

	if (listen_port == -1)
		return;

	log(LL_INFO, "APRS-SI: started thread");

	pollfd fds[] = { { fd, POLLIN, 0 } };

        for(;!terminate;) {
                try {
			int rc = poll(fds, 1, END_CHECK_INTERVAL_ms);
			
			if (rc == 0)
				continue;

			if (rc == -1) {
				log(LL_ERROR, "tranceiver_axudp::operator: poll returned %s", strerror(errno));

				break;
			}

			constexpr int       max_pkt_len { 1600 };
                        char               *buffer      = reinterpret_cast<char *>(calloc(1, max_pkt_len));
                        struct sockaddr_in  clientaddr  { 0 };
                        socklen_t           len         = sizeof(clientaddr);

                        int n = recvfrom(fd, buffer, max_pkt_len, 0, (sockaddr *)&clientaddr, &len);

                        if (n) {
				std::string came_from = inet_ntoa(clientaddr.sin_addr) + myformat(":%d", clientaddr.sin_port);

				timeval tv { 0 };
				gettimeofday(&tv, nullptr);

				std::string source = myformat("axudp(%s)", get_id().c_str());
				uint64_t    msg_id = get_random_uint64_t();

				message m(tv,
						source,
						msg_id,
						false, 
						0,
						reinterpret_cast<const uint8_t *>(buffer),
						n - 2 /* "remove" crc */);

				log(LL_DEBUG_VERBOSE, "tranceiver_axudp::operator(%s): received message from %s", m.get_id_short().c_str(), came_from.c_str());

				// if an error occured, do not pass on to
				transmit_error_t rc = queue_incoming_message(m);

				if (distribute && rc != TE_ratelimiting) {
					message m_full(tv,
						source,
						msg_id,
						false, 
						0,
						reinterpret_cast<const uint8_t *>(buffer),
						n);

					send_to_other_axudp_targets(m_full, came_from);
				}
			}

			free(buffer);
                }
                catch(const std::exception& e) {
                        log(LL_ERROR, "tranceiver_axudp::operator: recvfrom failed: %s", e.what());
                }
        }
}

tranceiver *tranceiver_axudp::instantiate(const libconfig::Setting & node_in, work_queue_t *const w, const position_t & pos)
{
	std::string               id;
	seen                     *s                 = nullptr;
	int                       listen_port       = -1;
	std::vector<std::string>  destinations;
	bool                      continue_on_error = false;
	bool                      distribute        = false;

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "incoming-rate-limiting") {
			assert(s == nullptr);
			s = seen::instantiate(node);
		}
		else if (type == "destinations") {
			std::string d = node_in.lookup(type).c_str();

			destinations = split(d, " ");
		}
		else if (type == "continue-on-error")
			continue_on_error = node_in.lookup(type);
		else if (type == "listen-port")
			listen_port = node_in.lookup(type);
		else if (type == "distribute")
			distribute = node_in.lookup(type);
		else if (type != "type") {
			error_exit(false, "setting \"%s\" is not known", type.c_str());
		}
        }

	return new tranceiver_axudp(id, s, w, pos, listen_port, destinations, continue_on_error, distribute);
}
