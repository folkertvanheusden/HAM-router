#include "config.h"
#include <math.h>
#include <string>
#include <string.h>
#if GPS_FOUND == 1
#include <libgpsmm.h>
#endif

#include "error.h"
#include "gps.h"
#include "log.h"
#include "str.h"
#include "time.h"


// https://stackoverflow.com/questions/27126714/c-latitude-and-longitude-distance-calculator
#define RADIO_TERRESTRE 6372797.56085
#define GRADOS_RADIANES M_PI / 180.0
#define RADIANES_GRADOS 180.0 / M_PI

double calcGPSDistance(double latitude_new, double longitude_new, double latitude_old, double longitude_old)
{
    double  lat_new = latitude_old * GRADOS_RADIANES;
    double  lat_old = latitude_new * GRADOS_RADIANES;
    double  lat_diff = (latitude_new-latitude_old) * GRADOS_RADIANES;
    double  lng_diff = (longitude_new-longitude_old) * GRADOS_RADIANES;

    double  a = sin(lat_diff/2) * sin(lat_diff/2) +
                cos(lat_new) * cos(lat_old) *
                sin(lng_diff/2) * sin(lng_diff/2);
    double  c = 2 * atan2(sqrt(a), sqrt(1-a));

    double  distance = RADIO_TERRESTRE * c;

    return distance;
}

// from https://stackoverflow.com/questions/36254363/how-to-convert-latitude-and-longitude-of-nmea-format-data-to-decimal
double convertToDecimalDegrees(const char *latLon, const char direction)
{
	char deg[4] = { 0 };
	const char *dot = nullptr, *min = nullptr;
	int len;
	double dec = 0;

	if ((dot = strchr(latLon, '.')))
	{                                         // decimal point was found
		min = dot - 2;                          // mark the start of minutes 2 chars back
		len = min - latLon;                     // find the length of degrees
		strncpy(deg, latLon, len);              // copy the degree string to allow conversion to float
		dec = atof(deg) + atof(min) / 60;       // convert to float
		if (direction == 'S' || direction == 'W')
			dec *= -1;
	}

	return dec;
}

void parse_nmea_pos(const char *what, double *const lat, double *const lng)
{
	*lat = *lng = 0.;

	if (what[0] == '@') {  // ignore time code
		what += 7;
		// TODO
	}
	else if (what[0] == '!') {  // straight away position
		what++;

		*lat = convertToDecimalDegrees(what, what[8]);

		what += 9;
		*lng = convertToDecimalDegrees(what, what[8]);
	}
}

std::string gps_double_to_aprs(const double lat, const double lng)
{
        double lata = abs(lat);
        double latd = floor(lata);
        double latm = (lata - latd) * 60;
        double lath = (latm - floor(latm)) * 100;
        double lnga = abs(lng);
        double lngd = floor(lnga);
        double lngm = (lnga - lngd) * 60;
        double lngh = (lngm - floor(lngm)) * 100;

        return myformat("%02d%02d.%02d%c/%03d%02d.%02d%c",
                        int(latd), int(floor(latm)), int(floor(lath)), lat > 0 ? 'N' : 'S',
                        int(lngd), int(floor(lngm)), int(floor(lngh)), lng > 0 ? 'E' : 'W');
}

gps_connector::gps_connector(const std::string & host, const int port, const std::optional<position_t> & default_position) :
	default_position(default_position),
	host(host),
	port(port)
{
#if GPS_FOUND == 1
	if (!host.empty())
		th = new std::thread(std::ref(*this));

	log(LL_INFO, "gps_connector instantiated");
#endif
}

gps_connector::~gps_connector()
{
	if (th) {
		terminate = true;

		th->join();
		delete th;

#if GPS_FOUND == 1
		delete gps_instance;
#endif
	}
}

void gps_connector::operator()()
{
#if GPS_FOUND == 1
	bool has_fix = false;

	log(LL_DEBUG, "gps_connector thread started");

	for(;!terminate;) {
		if (gps_instance == nullptr) {
			gps_instance = new gpsmm(host.c_str(), myformat("%d", port).c_str());

			if (gps_instance->stream(WATCH_ENABLE | WATCH_JSON) == nullptr)
				error_exit(false, "GSPD cannot be contacted");
		}

		if (!gps_instance->waiting(100000))  // wait 100ms for gps-data
			continue;

		gps_data_t * gpsd_data = gps_instance->read();
		if (gpsd_data == nullptr) {
			log(LL_WARNING, "Lost connection to gpsd");

			delete gps_instance;
			gps_instance = nullptr;

			myusleep(1000000, &terminate);

			has_fix          = false;

			continue;
		}

		if (gpsd_data->fix.mode < MODE_2D) {
			current_position = { };

			has_fix          = false;

			continue;
		}

		current_position = { gpsd_data->fix.latitude, gpsd_data->fix.longitude };

		if (!has_fix) {
			has_fix = true;

			log(LL_INFO, "GPS has fix: %f,%f", current_position.value().latitude, current_position.value().longitude);
		}
	}
#endif
}

std::optional<position_t> gps_connector::get_position()
{
	if (current_position.has_value())
		return current_position;

	return default_position;
}

gps_connector * gps_connector::instantiate(const libconfig::Setting & node_in)
{
        bool        lat_set   = false;
        bool        lng_set   = false;
	std::string gpsd_host;
	int         gpsd_port = 2947;
	double      latitude  = 0.;
	double      longitude = 0.;

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

                std::string type = node.getName();

                if (type == "local-latitude")
                        latitude = node_in.lookup(type),  lat_set = true;
                else if (type == "local-longitude")
                        longitude = node_in.lookup(type), lng_set = true;
                else if (type == "gpsd-host")
                        gpsd_host = node_in.lookup(type).c_str();
                else if (type == "gpsd-port")
                        gpsd_port = node_in.lookup(type);
                else {
                        error_exit(false, "General setting \"%s\" is not known", type.c_str());
                }
        }

        if (lat_set != lng_set)
                error_exit(false, "General settings: either latitude or longitude is not set");

        if (lat_set && latitude == 0. && longitude == 0.)
                error_exit(false, "General settings: suspicious global longitude/latitude set");

	std::optional<position_t> pos;

	if (lat_set)
		pos = { latitude, longitude };

	return new gps_connector(gpsd_host, gpsd_port, pos);
}
