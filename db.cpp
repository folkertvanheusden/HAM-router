// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#include <streambuf>
#include <string>
#include <vector>
#include <mysql_connection.h>

#include "db.h"
#include "log.h"
#include "utils.h"

// libmysqlcppconn-dev


class DataBuf : public std::streambuf
{
	public:
		DataBuf(char *d, size_t s) {
			std::streambuf::setg(d, d, d + s);
		}
};


bool db::check_table_exists(const std::string & table)
{
	sql::Statement *stmt = con->createStatement();
	sql::ResultSet *res = stmt->executeQuery(myformat("show tables like \"%s\"", table.c_str()).c_str());
	bool rc = res->next();
	delete res;
	delete stmt;

	return rc;
}

void db::log_sql_exception(const std::string query, const sql::SQLException & e)
{
	log(LL_ERR, "%s MySQL error: %s, error code: %d", query.c_str(), e.what(), e.getErrorCode());
}

void db::reconnect()
{
	if (driver) {
		if (con) {
			try {
				sql::Statement *stmt = con->createStatement();
				sql::ResultSet *res = stmt->executeQuery("SELECT 1");
				if (res) {
					res->next();
					delete res;
				}
				delete stmt;
			}
			catch(sql::SQLException & e) {
				log_sql_exception("MySQL ping", e);

				delete con;
				con = nullptr;
			}
		}

		if (!con && !url.empty()) {
			try {
				sql::ConnectOptionsMap connection_properties;
				connection_properties["hostName"] = url;
				connection_properties["userName"] = username;
				connection_properties["password"] = password;

				con = driver->connect(url.c_str(), username.c_str(), password.c_str());

				bool myTrue = true;
				con->setClientOption("OPT_RECONNECT", &myTrue);
			}
			catch(sql::SQLException & e) {
				log_sql_exception("MySQL reconnect", e);

				delete con;
				con = nullptr;
			}
		}
	}
}

db::db(const std::string & url, const std::string & username, const std::string & password) : url(url), username(username), password(password)
{
	if (url.empty()) {
		log(LL_INFO, "No database configured");
		return;
	}

	try {
		driver = get_driver_instance();

		reconnect();

		if (!check_table_exists("APRS")) {
			sql::Statement *stmt = con->createStatement();
			stmt->execute("CREATE TABLE APRS(ts datetime not null, rssi double, snr double, crc int(1) not null, content blob, latitude double, longitude double, distance double, callsign_to varchar(9), callsign_from varchar(9))");
			delete stmt;
		}
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(create tables)", e);
	}
}

db::~db()
{
	delete con;
}


void db::insert_message(uint8_t *msg, int msg_size, double rssi, double snr, int crc, double latitude, double longitude, double distance, std::string callsign_to, std::string callsign_from)
{
	if (!driver)
		return;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	sql::PreparedStatement *stmt { nullptr };

	try {
		stmt = con->prepareStatement("INSERT INTO APRS(ts, rssi, snr, crc, content, latitude, longitude, distance, callsign_to, callsign_from) VALUES(NOW(), ?, ?, ?, ?, ?, ?, ?, ?, ?)");

		stmt->setDouble(1, rssi);
		stmt->setDouble(2, snr);
		stmt->setInt(3, crc);

		DataBuf buffer(reinterpret_cast<char *>(msg), msg_size);
		std::istream stream(&buffer);
		stmt->setBlob(4, &stream);

		stmt->setDouble(5, latitude);
		stmt->setDouble(6, longitude);
		stmt->setDouble(7, distance);
		stmt->setString(8, callsign_to);
		stmt->setString(9, callsign_from);
		stmt->execute();
	}
	catch(sql::SQLException & e) {
		log_sql_exception("insert_message", e);
	}

	delete stmt;
}
