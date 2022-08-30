#include <time.h>


class rate_limiter
{
private:
	const int    max_per_dt { 0 };
	const double dt         { 0 };

	double       last_ts    { 0 };
	double       allowance  { 0 };

public:
	rate_limiter(const int max_per_dt, const double dt);
	~rate_limiter();

	bool check();

	time_t get_last_ts() const { return time_t(last_ts); }
};
