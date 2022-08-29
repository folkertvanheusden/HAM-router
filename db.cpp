// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#include <streambuf>
#include <string>
#include <vector>
#include <mysql_connection.h>

#include "db.h"
#include "log.h"
#include "str.h"

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
			stmt->execute("CREATE TABLE APRS(ts datetime not null, rssi double, snr double, crc int(1) not null, content blob, latitude double, longitude double, distance double, callsign_to varchar(32), callsign_from varchar(32))");
			delete stmt;
		}

		if (!check_table_exists("airtime")) {
			sql::Statement *stmt = con->createStatement();
			stmt->execute("CREATE TABLE airtime(ts datetime not null, duration double not null, transmit int(1) not null, callsign varchar(32), primary key(ts))");
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

void db::insert_airtime(const double duration_ms, const bool transmit, const std::optional<std::string> & callsign)
{
	if (!driver)
		return;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	sql::PreparedStatement *stmt { nullptr };

	try {
		if (callsign.has_value()) {
			stmt = con->prepareStatement("INSERT INTO airtime(ts, duration, transmit, callsign) VALUES(NOW(), ?, ?, ?)");

			stmt->setDouble(1, duration_ms);
			stmt->setInt(2, int(transmit));
			stmt->setString(3, callsign.value());
		}
		else {
			stmt = con->prepareStatement("INSERT INTO airtime(ts, duration, transmit) VALUES(NOW(), ?, ?)");

			stmt->setDouble(1, duration_ms);
			stmt->setInt(2, int(transmit));
		}

		stmt->execute();
	}
	catch(sql::SQLException & e) {
		log_sql_exception("insert_airtime", e);
	}

	delete stmt;
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

std::pair<std::vector<std::string>, std::vector<std::vector<std::string> > > db::get_airtime_per_callsign()
{
	std::vector<std::string> what;
	what.push_back("callsign");
	what.push_back("tx/rx");
	what.push_back("avg duration (ms)");
	what.push_back("avg duration (%)");
	what.push_back("# records");

	std::vector<std::vector<std::string> > records;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	sql::PreparedStatement *stmt { nullptr };
	sql::ResultSet *res { nullptr };

	try {
		std::string query_get = "select callsign, transmit, avg(duration) as avg_duration, sum(n) as nrec from (select callsign, sum(duration) as duration, count(*) as n, transmit from airtime group by callsign, transmit, hour(ts)) as in_ group by callsign, transmit";

		stmt = con->prepareStatement(query_get);
		res = stmt->executeQuery();

		while(res->next()) {
			std::vector<std::string> record;
			record.push_back(res->getString("callsign"));
			record.push_back(res->getInt("transmit") ? "transmit" : "receive");
			double avg_duration = res->getDouble("avg_duration");
			record.push_back(myformat("%.2f", avg_duration));
			record.push_back(myformat("%.2f", (avg_duration * 100) / 86400000));
			record.push_back(myformat("%u", res->getInt("nrec")));

			records.push_back(record);
		}
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(purge)", e);
	}

	delete res;
	delete stmt;

	return { what, records };
}
