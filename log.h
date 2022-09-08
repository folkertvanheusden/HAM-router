// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <string>


#define LL_FATAL	0
#define LL_ERROR	1
#define LL_WARNING	2
#define LL_INFO		3
#define LL_DEBUG	4
#define LL_DEBUG_VERBOSE	5

std::string ll_to_str(const int ll);
int get_default_loglevel();
void setlogfile(const char *const other, const int loglevel);
void unsetlogfile();
void log(const int loglevel, const char *const what, ...);
void lognee(const int loglevel, const char *const what, ...);
void log(const std::string & id, const int loglevel, const char *const what, ...);
void log(const int loglevel, const std::string & what, ...);
