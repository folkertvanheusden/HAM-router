#include "tranceiver-kiss.h"


class tranceiver_kiss_tty : public tranceiver_kiss
{
public:
	tranceiver_kiss_tty(const std::string & id, seen *const s, work_queue_t *const w, const position_t & pos, const std::string & tty_path, const int tty_bps);
	virtual ~tranceiver_kiss_tty();

	std::string get_type_name() const override { return "KISS-tty"; }

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w, const position_t & pos);
};
