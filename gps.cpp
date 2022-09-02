#include <math.h>
#include <string>
#include <string.h>

#include "str.h"

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

