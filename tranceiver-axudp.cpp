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

	for(auto p : peers) {
		mlog(LL_DEBUG_VERBOSE, m, "put_message_low", myformat("transmit to %s (%s)", p.first.c_str(), dump_replace(temp, temp_len).c_str()));

		if (transmit_udp(p.first, temp, temp_len) == false && continue_on_error == false) {
			mlog(LL_WARNING, m, "put_message_low", "problem sending");

			free(temp);

			return TE_hardware;
		}
	}

	free(temp);

	return TE_ok;
}

tranceiver_axudp::tranceiver_axudp(const std::string & id, seen *const s, work_queue_t *const w, gps_connector *const gps, const int listen_port, const std::vector<std::pair<std::string, filter *> > & peers, const bool continue_on_error, const bool distribute) :
	tranceiver(id, s, w, gps),
	listen_port(listen_port),
	peers(peers),
	continue_on_error(continue_on_error),
	distribute(distribute)
{
	log(LL_INFO, "Instantiated AXUDP");

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

        struct sockaddr_in servaddr { 0 };

        servaddr.sin_family      = AF_INET; // IPv4
        servaddr.sin_addr.s_addr = INADDR_ANY;
        servaddr.sin_port        = htons(listen_port);

        if (bind(fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
                error_exit(true, "axudp(%s) bind to port %d failed", get_id().c_str(), listen_port);

	th = new std::thread(std::ref(*this));
}

tranceiver_axudp::~tranceiver_axudp()
{
	close(fd);
}

transmit_error_t tranceiver_axudp::send_to_other_axudp_targets(const message & m, const std::string & came_from)
{
	for(auto p : peers) {
		if (p.first == came_from) {
			mlog(LL_DEBUG_VERBOSE, m, "send_to_other_axudp_targets", myformat("not (re-)sending to %s", p.first.c_str()));

			continue;
		}

		if (p.second == nullptr || p.second->check(m)) {
			mlog(LL_DEBUG_VERBOSE, m, "send_to_other_axudp_targets", myformat("transmit to %s", p.first.c_str()));

			auto content = m.get_content();

			if (transmit_udp(p.first, content.first, content.second) == false && continue_on_error == false) {
				mlog(LL_WARNING, m, "send_to_other_axudp_targets", "problem sending");

				return TE_hardware;
			}
		}
		else {
			mlog(LL_DEBUG, m, "send_to_other_axudp_targets", myformat("not sending to %s due to filter", p.first.c_str()));
		}
	}

	return TE_ok;
}

void tranceiver_axudp::operator()()
{
	set_thread_name("t-axudp");

	if (listen_port == -1)
		return;

	log(LL_INFO, "started thread");

	pollfd fds[] = { { fd, POLLIN, 0 } };

        for(;!terminate;) {
                try {
			int rc = poll(fds, 1, END_CHECK_INTERVAL_ms);
			
			if (rc == 0)
				continue;

			if (rc == -1) {
				log(LL_ERROR, myformat("poll returned %s", strerror(errno)));

				break;
			}

			constexpr int       max_pkt_len { 1600 };
                        char               *buffer      = reinterpret_cast<char *>(calloc(1, max_pkt_len));
                        struct sockaddr_in  clientaddr  { 0 };
                        socklen_t           len         = sizeof(clientaddr);

                        int n = recvfrom(fd, buffer, max_pkt_len, 0, (sockaddr *)&clientaddr, &len);

                        if (n > 2) {
				std::string came_from = inet_ntoa(clientaddr.sin_addr) + myformat(":%d", clientaddr.sin_port);

				timeval tv { 0 };
				gettimeofday(&tv, nullptr);

				std::string source = myformat("axudp(%s)", get_id().c_str());
				uint64_t    msg_id = get_random_uint64_t();

				message m(tv,
						source,
						msg_id,
						reinterpret_cast<const uint8_t *>(buffer),
						n - 2 /* "remove" crc */);

				mlog(LL_DEBUG_VERBOSE, m, "operator", "received message from " + came_from);

				// if an error occured, do not pass on to
				transmit_error_t rc = queue_incoming_message(m);

				if (distribute && rc != TE_ratelimiting) {
					message m_full(tv,
						source,
						msg_id,
						reinterpret_cast<const uint8_t *>(buffer),
						n);

					send_to_other_axudp_targets(m_full, came_from);
				}
			}
			else if (n == -1) {
				log(LL_WARNING, myformat("recvfrom returned %s", strerror(errno)));
			}

			free(buffer);
                }
                catch(const std::exception& e) {
                        log(LL_ERROR, myformat("recvfrom failed: %s", e.what()));
                }
        }
}

tranceiver *tranceiver_axudp::instantiate(const libconfig::Setting & node_in, work_queue_t *const w, gps_connector *const gps, const std::map<std::string, filter *> & filters)
{
	std::string  id;
	seen        *s                 = nullptr;
	int          listen_port       = -1;
	std::vector<std::pair<std::string, filter *> > peers;
	bool         continue_on_error = false;
	bool         distribute        = false;

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "repetition-rate-limiting") {
			if (s)
				error_exit(false, "axudp(line %d): repetition-rate-limiting already defined", node.getSourceLine());

			s = seen::instantiate(node);
		}
		else if (type == "peers") {
			for(int j=0; j<node.getLength(); j++) {
				const libconfig::Setting & peer_node = node[j];

				std::string  host;
				filter      *f    = nullptr;

				for(int k=0; k<peer_node.getLength(); k++) {
					const libconfig::Setting & peer_setting = peer_node[k];

					std::string peer_setting_type = peer_setting.getName();

					if (peer_setting_type == "host")
						host = peer_node.lookup(peer_setting_type).c_str();
					else if (peer_setting_type == "filter") {
						std::string filter_name = peer_node.lookup(peer_setting_type).c_str();

						auto it_f = filters.find(filter_name);

						if (it_f == filters.end())
							error_exit(false, "axudp(line %d): filter \"%s\" is not defined", peer_setting.getSourceLine(), filter_name.c_str());

						f = it_f->second;
					}
					else {
						error_exit(false, "axudp(line %d): unknown peer type '%s'", peer_setting.getSourceLine(), peer_setting_type.c_str());
					}
				}

				if (host.empty())
					error_exit(false, "axudp(line %d): host not defined", peer_node.getSourceLine());

				peers.push_back({ host, f });
			}
		}
		else if (type == "continue-on-error")
			continue_on_error = node_in.lookup(type);
		else if (type == "listen-port")
			listen_port = node_in.lookup(type);
		else if (type == "distribute")
			distribute = node_in.lookup(type);
		else if (type != "type") {
			error_exit(false, "axudp(line %d): setting \"%s\" is not known", node.getSourceLine(), type.c_str());
		}
        }

	return new tranceiver_axudp(id, s, w, gps, listen_port, peers, continue_on_error, distribute);
}
