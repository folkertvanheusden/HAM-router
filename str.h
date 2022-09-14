#include <string>
#include <vector>


std::string myformat(const char *const fmt, ...);
std::string dump_hex(const unsigned char *p, int len);
std::string dump_replace(const unsigned char *p, int len);
std::vector<std::string> split(std::string in, std::string splitter);
std::string trim(std::string in, const std::string what = " ");
