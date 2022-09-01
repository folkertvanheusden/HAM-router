#include "data.h"
#include "utils.h"


data::data(const uint8_t *const d, const size_t size) :
	d(reinterpret_cast<uint8_t *>(duplicate(d, size))),
	size(size)
{
}


data::data(const data & d_in) :
	d   (reinterpret_cast<uint8_t *>(duplicate(d_in.get_data(), d_in.get_size()))),
	size(d_in.get_size())
{
}

data::~data()
{
	free(const_cast<uint8_t *>(d));
}
