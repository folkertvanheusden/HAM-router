#pragma once

#include "config.h"
#include <atomic>
#include <libconfig.h++>
#include <optional>
#include <string>
#include <thread>
#if GPS_FOUND == 1
#include <libgpsmm.h>
#endif


typedef struct
{
	double latitude;
	double longitude;
} position_t;

class gps_connector
{
private:
	std::optional<position_t> default_position { };
	std::optional<position_t> current_position { };

	std::string               host;
	int                       port { 2947 };

	std::atomic_bool          terminate        { false   };
	std::thread              *th               { nullptr };

#if GPS_FOUND == 1
	gpsmm                    *gps_instance     { nullptr };
#endif

public:
	gps_connector(const std::string & host, const int port, const std::optional<position_t> & default_position);
	virtual ~gps_connector();

	std::optional<position_t> get_position();

	void operator()();

	static gps_connector * instantiate(const libconfig::Setting & node);
};

double calc_gps_distance(double latitude_new, double longitude_new, double latitude_old, double longitude_old);
std::optional<std::pair<double, double> > parse_nmea_pos(const char *what);
std::string gps_double_to_aprs(const double lat, const double lng);
