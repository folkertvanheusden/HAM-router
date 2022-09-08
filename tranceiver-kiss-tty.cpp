#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <optional>
#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "error.h"
#include "log.h"
#include "net.h"
#include "random.h"
#include "str.h"
#include "time.h"
#include "tranceiver-kiss-tty.h"
#include "utils.h"


tranceiver_kiss_tty::tranceiver_kiss_tty(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos, const std::string & tty_path, const int tty_bps) :
	tranceiver_kiss(id, s, w, pos)
{
	log(LL_INFO, "Instantiating KISS-tty");

	fd = open(tty_path.c_str(), O_RDWR | O_NOCTTY);

	if (fd == -1)
		error_exit(true, "Failed to open tty (%s)", tty_path.c_str());

	termios tty     { 0 };
	termios tty_old { 0 };

	if (tcgetattr(fd, &tty) == -1)
		error_exit(true, "tcgetattr failed");

	tty_old = tty;

	speed_t speed = B9600;

	if (tty_bps == 9600) {
	}
	else if (tty_bps == 115200) {
		speed = B115200;
	}

	cfsetospeed(&tty, speed);

	tty.c_cflag &= ~PARENB;           // 8N1
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |=  CS8;

	tty.c_cflag &= ~CRTSCTS;         // no flow control
	tty.c_cflag |=  CREAD | CLOCAL;  // ignore control lines

	cfmakeraw(&tty);

	tcflush(fd, TCIFLUSH);

	if (tcsetattr(fd, TCSANOW, &tty) != 0)
		error_exit(true, "tcsetattr failed");

	th = new std::thread(std::ref(*this));
}

tranceiver_kiss_tty::~tranceiver_kiss_tty()
{
}

tranceiver *tranceiver_kiss_tty::instantiate(const libconfig::Setting & node_in, work_queue_t *const w, const position_t & pos)
{
	std::string  id;
	seen        *s            = nullptr;
	std::string  tty_device;
	int          tty_baudrate = 9600;

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "repetition-rate-limiting") {
			if (s)
				error_exit(false, "(line %d): Duplicate repetition-rate-limiting in tranceiver-kiss-tty (%s)", node.getSourceLine(), id.c_str());

			s = seen::instantiate(node);
		}
		else if (type == "tty-device")
			tty_device = node_in.lookup(type).c_str();
		else if (type == "tty-baudrate")
			tty_baudrate = node_in.lookup(type);
		else if (type != "type") {
			error_exit(false, "(line %d): setting \"%s\" is not known", node.getSourceLine(), type.c_str());
		}
        }

	return new tranceiver_kiss_tty(id, s, w, pos, tty_device, tty_baudrate);
}
