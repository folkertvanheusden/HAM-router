// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <mutex>
#include <string>
#include <vector>


class db
{
private:
	std::mutex lock;
	std::string url, username, password;

	sql::Driver *driver { nullptr };
	sql::Connection *con { nullptr };

	bool check_table_exists(const std::string & table);
	void log_sql_exception(const std::string query, const sql::SQLException & e);

	void reconnect();

public:
	db(const std::string & url, const std::string & username, const std::string & password);
	~db();

	bool using_db() const { return con != nullptr; }

	void insert_message(uint8_t *msg, int msg_size, double rssi, double snr, int crc, double latitude, double longitude, double distance);
};
