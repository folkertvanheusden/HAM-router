#include <stdint.h>
#include <string>

#define MAX_PACKET_SIZE 254

int  connect_to(const char *host, const int portnr);
bool transmit_udp(const std::string & dest, const uint8_t *const data, const size_t data_len);
int  WRITE(int fd, const uint8_t *whereto, size_t len);

void startiface(const char *const dev);
int  setifcall(const int fd, const char *const name);

std::string get_ax25_addr(const uint8_t *const in);

uint16_t get_net_short(const uint8_t *const p);
uint32_t get_net_long(const uint8_t *const p);
uint64_t get_net_long_long(const uint8_t *const p);

void put_net_long(uint8_t *const p, const uint32_t v);
void put_net_long_long(uint8_t *const p, const uint64_t v);
