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
#include "tranceiver-kiss-kernel.h"
#include "utils.h"


tranceiver_kiss_kernel::tranceiver_kiss_kernel(const std::string & id, seen *const s, work_queue_t *const w, gps_connector *const gps, const std::string & callsign, const std::string & if_up) :
	tranceiver_kiss(id, s, w, gps)
{
	log(LL_INFO, "Instantiating KISS-kernel");

	int fd_master = -1;
	int fd_slave  = -1;

	log(LL_INFO, "Configuring kiss interface");

	if (openpty(&fd_master, &fd_slave, NULL, NULL, NULL) == -1)
		error_exit(true, "openpty failed");

	int disc = N_AX25;
	if (ioctl(fd_slave, TIOCSETD, &disc) == -1)
		error_exit(true, "error setting line discipline");

	if (setifcall(fd_slave, callsign.c_str()) == -1)
		error_exit(false, "cannot set call");

	int v = 4;
	if (ioctl(fd_slave, SIOCSIFENCAP, &v) == -1)
		error_exit(true, "failed to set encapsulation");

	char dev_name[64] = { 0 };
	if (ioctl(fd_slave, SIOCGIFNAME, dev_name) == -1)
		error_exit(true, "failed retrieving name of ax25 network device name");

	startiface(dev_name);

	if (if_up.empty() == false)
		system((if_up + " " + dev_name).c_str());

	fd = fd_master;

	th = new std::thread(std::ref(*this));
}

tranceiver_kiss_kernel::~tranceiver_kiss_kernel()
{
}

tranceiver *tranceiver_kiss_kernel::instantiate(const libconfig::Setting & node_in, work_queue_t *const w, gps_connector *const gps)
{
	std::string  id;
	seen        *s = nullptr;
	std::string  callsign;
	std::string  if_up;

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "repetition-rate-limiting") {
			if (s)
				error_exit(false, "(line %d): repetition-rate-limiting already defined", node.getSourceLine());

			s = seen::instantiate(node);
		}
		else if (type == "callsign")
			callsign = node_in.lookup(type).c_str();
		else if (type == "if-up")
			if_up = node_in.lookup(type).c_str();
		else if (type != "type") {
			error_exit(false, "(line %d): setting \"%s\" is not known", node.getSourceLine(), type.c_str());
		}
        }

	if (callsign.empty())
		error_exit(false, "(line %d): No callsign selected", node_in.getSourceLine());

	return new tranceiver_kiss_kernel(id, s, w, gps, callsign, if_up);
}
