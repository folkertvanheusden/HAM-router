// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <time.h>
#include <sys/time.h>

#include "error.h"
#include "log.h"
#include "str.h"

static const char *logfile = nullptr;
static int loglevel = LL_INFO;

int get_default_loglevel()
{
	return loglevel;
}

void setlogfile(const char *const file, const int ll)
{
	logfile = strdup(file);
	loglevel = ll;
}

void unsetlogfile()
{
	free(const_cast<char *>(logfile));

	logfile = nullptr;
}

std::string get_thread_name()
{
	char buffer[16] { 0 };

	pthread_getname_np(pthread_self(), buffer, sizeof buffer);

	return buffer;
}

void _log(const std::string & id, const int ll, const char *const what, va_list args, bool ee)
{
	if (ll > loglevel)
		return;

	struct timeval tv;
	gettimeofday(&tv, nullptr);

	time_t tv_temp = tv.tv_sec;
	struct tm tm;
	localtime_r(&tv_temp, &tm);

	char *msg = nullptr;
	if (vasprintf(&msg, what, args) == -1)
		error_exit(true, "_log: vasprintf failed\n");

	const char *lls = "???";
	switch(ll) {
		case LL_FATAL:
			lls = "FATAL";
			break;
		case LL_ERROR:
			lls = "ERROR";
			break;
		case LL_WARNING:
			lls = "WARN";
			break;
		case LL_INFO:
			lls = "INFO";
			break;
		case LL_DEBUG:
			lls = "DEBUG";
			break;
		case LL_DEBUG_VERBOSE:
			lls = "DEBVR";
			break;
	}

	char *temp = nullptr;
	if (asprintf(&temp, "%04d-%02d-%02d %02d:%02d:%02d.%06ld %5s %9s %s %s", 
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec,
			lls, get_thread_name().c_str(), id.c_str(),
			msg) == -1)
		error_exit(true, "_log: asprintf failed\n");

	free(msg);

	if (logfile) {
		FILE *fh = fopen(logfile, "a+");
		if (!fh) {
			if (ee)
				error_exit(true, "Cannot access logfile '%s'", logfile);

			fprintf(stderr, "Cannot write to logfile %s (%s)\n", logfile, temp);

			exit(1);
		}

		fprintf(fh, "%s\n", temp);
		fclose(fh);
	}

	printf("%s\n", temp);
	free(temp);
}

void log(const int ll, const std::string what, ...)
{
	va_list ap;
	va_start(ap, what);

	_log("", ll, what.c_str(), ap, true);

	va_end(ap);
}

void log(const int ll, const char *const what, ...)
{
	va_list ap;
	va_start(ap, what);

	_log("", ll, what, ap, true);

	va_end(ap);
}

void lognee(const int ll, const char *const what, ...)
{
	va_list ap;
	va_start(ap, what);

	_log("", ll, what, ap, false);

	va_end(ap);
}


void log(const std::string & id, const int ll, const char *const what, ...)
{
	va_list ap;
	va_start(ap, what);

	_log(myformat("[%s]", id.c_str()), ll, what, ap, true);

	va_end(ap);
}

std::string ll_to_str(const int ll)
{
	if (ll == 0)
		return "fatal";

	if (ll == 1)
		return "error";

	if (ll == 2)
		return "warning";

	if (ll == 3)
		return "info";

	if (ll == 4)
		return "debug";

	if (ll == 5)
		return "debug-verbose";

	return "?";
}

void log_regexp_error(int rc, regex_t *const re, const std::string & what)
{
	char errbuf[128] { 0 };
	regerror(rc, re, errbuf, sizeof errbuf);

	log(LL_ERROR, "Problem executing regular expression (%s): %s", what.c_str(), errbuf);
}
