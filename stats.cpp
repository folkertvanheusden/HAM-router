// (C) 2020-2022 by folkert van heusden <mail@vanheusden.com>, released under Apache License v2.0
#include <assert.h>
#include <cstring>
#include <fcntl.h>
#include <iterator>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "error.h"
#include "log.h"
#include "stats.h"
#include "str.h"

constexpr char shm_name[] = "/lora-aprs-gw";

void stats_inc_counter(uint64_t *const p)
{
	if (!p)
		return;

#if defined(GCC_VERSION) && GCC_VERSION >= 40700
	__atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST);
#else
	(*p)++; // hope for the best
#endif
}

void stats_add_counter(uint64_t *const p, const uint64_t value)
{
	if (!p)
		return;

#if defined(GCC_VERSION) && GCC_VERSION >= 40700
	__atomic_add_fetch(p, value, __ATOMIC_SEQ_CST);
#else
	(*p) += value; // hope for the best
#endif
}

void stats_set(uint64_t *const p, const uint64_t value)
{
	if (!p)
		return;

	__atomic_store(p, &value, __ATOMIC_SEQ_CST);
}

void stats_add_average(uint64_t *const p, const int val)
{
	if (!p)
		return;

#if defined(GCC_VERSION) && GCC_VERSION >= 40700
	// there's a window where these values are
	// not in sync
	__atomic_add_fetch(p + 1, 1, __ATOMIC_SEQ_CST);
	__atomic_add_fetch(p, val, __ATOMIC_SEQ_CST);
#else
	// hope for the best
	(*(p + 1))++;
	(*p) += val;
#endif
}

stats::stats(const int size, snmp_data *const sd) :
	size(size),
	sd(sd)
{
	fd = shm_open(shm_name, O_RDWR | O_CREAT, 0644);
	if (fd == -1)
		error_exit(true, "stats: shm_open: %s", strerror(errno));

	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		error_exit(true, "stats: fcntl(FD_CLOEXEC)");

	if (ftruncate(fd, size) == -1)
		error_exit(true, "stats: truncate");

	p = (uint8_t *)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED)
		error_exit(true, "stats: mmap");

	memset(p, 0x00, size);

	close(fd);
}

stats::~stats()
{
	log(LL_DEBUG, "Removing shared memory segment\n");
	munmap(p, size);

	shm_unlink(shm_name);
}

uint64_t * stats::register_stat(const std::string & name, const std::string & oid, const snmp_integer::snmp_integer_type type)
{
	log(LL_DEBUG_VERBOSE, "Registering statistic %s on oid %s", name.c_str(), oid.c_str());

	if (len + 48 > size)
		error_exit(false, "stats: shm is full");

	std::unique_lock<std::mutex> lck(lock);

	auto lut_it = lut.find(name);
	if (lut_it != lut.end())
		error_exit(false, "stats: stat \"%s\" already exists", name.c_str());

	uint8_t *p_out = (uint8_t *)&p[len];

	stats_t s;
	s.p   = reinterpret_cast<uint64_t *>(p_out);
	s.oid = oid;

	lut.insert({ name, s});

	// hopefully this platform allows atomic updates
	// not using locking, for speed
	*(uint64_t *)p_out = 0;
	*(uint64_t *)(p_out + 8) = 0;

	int copy_n = std::min(name.size(), size_t(31));
	memcpy(&p_out[16], name.c_str(), copy_n);
	p_out[16 + copy_n] = 0x00;

	len += 48;

	if (oid.empty() == false)
		sd->register_oid(oid, new snmp_data_type_stats(type, reinterpret_cast<uint64_t *>(p_out)));

	return reinterpret_cast<uint64_t *>(p_out);
}

uint64_t * stats::find_stat(const std::string & name)
{
	std::unique_lock<std::mutex> lck(lock);

	auto lut_it = lut.find(name);
	if (lut_it != lut.end())
		return lut_it->second.p;

	return nullptr;
}

std::map<std::string, std::string> stats::snapshot()
{
	std::map<std::string, std::string> out;

	std::unique_lock<std::mutex> lck(lock);

	for(auto lut_it : lut)
		out.insert({ lut_it.first, myformat("%llu", (long long unsigned)*lut_it.second.p) });

	return out;
}
