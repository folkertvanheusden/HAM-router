#include "db.h"
#include "tranceiver.h"


class tranceiver_db : public tranceiver
{
private:
	db *const d { nullptr };

protected:
	transmit_error_t put_message_low(const message & m) override;

public:
	tranceiver_db(const std::string & id, seen *const s, work_queue_t *const w, db *const d);
	virtual ~tranceiver_db();

	std::string get_type_name() const override { return "MongoDB"; }

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w);

	void operator()() override;
};
