#include "config.h"
#if LIBMONGOCXX_FOUND == 1
#include <mongocxx/client.hpp>

#include "db.h"


class db_mongodb : public db
{
private:
	const std::string  database;
	const std::string  collection;
	mongocxx::client  *m_c  { nullptr };

public:
	db_mongodb(const std::string & uri, const std::string & database, const std::string & collection);
	virtual ~db_mongodb();

	void init_database() override;

	bool insert(const db_record & dr) override;

	std::vector<std::pair<std::string, uint32_t> > get_heard_counts() override;

	std::vector<std::pair<std::string, double  > > get_air_time()     override;
};
#endif
