// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <string>


void *duplicate(const void *in, const size_t size);

void  set_thread_name(const std::string & name);
