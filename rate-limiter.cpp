#include "rate-limiter.h"
#include "time.h"

double get_us_float()
{
	return get_us() / 1000000.0;
}

rate_limiter::rate_limiter(const int max_per_dt, const double dt) :
	max_per_dt(max_per_dt),
	dt(dt)
{
}

rate_limiter::~rate_limiter()
{
}

// http://stackoverflow.com/questions/667508/whats-a-good-rate-limiting-algorithm
bool rate_limiter::check()
{
	double now_ts = get_us_float();

	int    rate   = max_per_dt;     // unit: messages
	double per    = dt;	        // unit: seconds
	// allowance, unit: messages

	double last_check = last_ts;    // floating-point, e.g. usec accuracy. Unit: seconds

	double time_passed = now_ts - last_check;

	allowance += time_passed * (rate / per);

	last_ts = now_ts;

	if (allowance > rate)
		allowance = rate; // throttle

	if (allowance < 1.0)
		return false;

	allowance -= 1.0;

	return true;
}
