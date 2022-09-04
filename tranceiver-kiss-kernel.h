#include "tranceiver-kiss.h"


class tranceiver_kiss_kernel : public tranceiver_kiss
{
public:
	tranceiver_kiss_kernel(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos, const std::string & callsign, const std::string & if_up);
	virtual ~tranceiver_kiss_kernel();

	std::string get_type_name() const override { return "KISS-kernel"; }

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w, const position_t & pos);
};
