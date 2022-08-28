#include <errno.h>
#include <optional>
#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "error.h"
#include "log.h"
#include "net.h"
#include "tranceiver-axudp.h"
#include "utils.h"


transmit_error_t tranceiver_axudp::put_message_low(const uint8_t *const msg, const size_t len)
{
	for(auto d : destinations) {
		if (transmit_udp(d, msg, len) == false && continue_on_error == false) {
			log(LL_WARNING, "Problem sending");

			return TE_hardware;
		}
	}

	return TE_ok;
}

tranceiver_axudp::tranceiver_axudp(const std::string & id, seen *const s, work_queue_t *const w, const int listen_port, const std::vector<std::string> & destinations, const bool continue_on_error) :
	tranceiver(id, s, w),
	listen_port(listen_port),
	destinations(destinations),
	continue_on_error(continue_on_error)
{
	log(LL_INFO, "Instantiated AXUDP (%s)", id.c_str());

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

	th = new std::thread(std::ref(*this));
}

tranceiver_axudp::~tranceiver_axudp()
{
	close(fd);
}

void tranceiver_axudp::operator()()
{
	if (listen_port == -1)
		return;

	log(LL_INFO, "APRS-SI: started thread");

        struct sockaddr_in servaddr { 0 };

        servaddr.sin_family      = AF_INET; // IPv4
        servaddr.sin_addr.s_addr = INADDR_ANY;
        servaddr.sin_port        = htons(listen_port);

        if (bind(fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
                error_exit(true, "bind to port %d failed", listen_port);

        for(;;) {
                try {
                        char               *buffer     = reinterpret_cast<char *>(calloc(1, 1600));
                        struct sockaddr_in  clientaddr { 0 };
                        socklen_t           len        = sizeof(clientaddr);

                        int n = recvfrom(fd, buffer, sizeof buffer, 0, (sockaddr *)&clientaddr, &len);

                        if (n) {
				message_t m { 0 };
				gettimeofday(&m.tv, nullptr);
				m.message = reinterpret_cast<uint8_t *>(buffer);
				m.s       = len;

				queue_incoming_message(m);
			}
			else {
				free(buffer);
			}
                }
                catch(const std::exception& e) {
                        log(LL_ERR, "tranceiver_axudp::operator: recvfrom failed: %s", e.what());
                }
        }
}

tranceiver *tranceiver_axudp::instantiate(const libconfig::Setting & node_in, work_queue_t *const w)
{
	std::string               id;
	seen                     *s = nullptr;
	int                       listen_port = -1;
	std::vector<std::string>  destinations;
	bool                      continue_on_error;

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "incoming-rate-limiting")
			s = seen::instantiate(node);
		else if (type == "destinations") {
			std::string d = node_in.lookup(type).c_str();

			destinations = split(d, " ");
		}
		else if (type == "continue-on-error")
			continue_on_error = node_in.lookup(type);
		else if (type == "listen-port")
			listen_port = node_in.lookup(type);
		else if (type != "type") {
			error_exit(false, "setting \"%s\" is now known", type.c_str());
		}
        }

	return new tranceiver_axudp(id, s, w, listen_port, destinations, continue_on_error);
}
