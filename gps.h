#include <string>

double calcGPSDistance(double latitude_new, double longitude_new, double latitude_old, double longitude_old);
double convertToDecimalDegrees(const char *latLon, const char direction);
void parse_nmea_pos(const char *what, double *const lat, double *const lng);
std::string gps_double_to_aprs(const double lat, const double lng);
