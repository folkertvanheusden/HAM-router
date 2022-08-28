// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <stdint.h>
#include <string>
#include <vector>

uint64_t    get_us();
std::string myformat(const char *const fmt, ...);
std::string dump_hex(const unsigned char *p, int len);
std::string dump_replace(const unsigned char *p, int len);
std::vector<std::string> split(std::string in, std::string splitter);

uint32_t    calc_crc32(const uint8_t *const p, const size_t len);
