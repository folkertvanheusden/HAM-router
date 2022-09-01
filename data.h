#include <stdint.h>
#include <stdlib.h>
#include <utility>


class data
{
private:
	const uint8_t *d    { nullptr };
	const size_t   size { 0       };

public:
	data(const uint8_t *const d, const size_t size);

	data(const data & d);

	virtual ~data();

	const uint8_t *get_data() const { return d;    }

	size_t         get_size() const { return size; }

	std::pair<const uint8_t *, size_t> get_content() const { return { d, size }; }
};
