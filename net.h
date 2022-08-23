#include <stdint.h>
#include <string>

#define MAX_PACKET_SIZE 254

int WRITE(int fd, const uint8_t *whereto, size_t len);
int connect_to(const char *host, const int portnr);
void transmit_udp(const std::string & host, const int port, const uint8_t *const data, const size_t data_len);
void startiface(const char *dev);
std::string get_ax25_addr(const uint8_t *const in);
int setifcall(int fd, const char *name);
