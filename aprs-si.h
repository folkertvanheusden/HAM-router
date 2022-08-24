#include <mutex>
#include <string>

class aprs_si
{
private:
	int         fd { -1 };
	std::mutex  aprs_is_lock;
	std::string aprs_user;
	std::string aprs_pass;

protected:
	std::string receive_string(const int fd);

public:
	aprs_si(const std::string & aprs_user, const std::string & aprs_pass);
	~aprs_si();

	bool send_through_aprs_is(const std::string & content_out);
};
