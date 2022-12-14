#pragma once

#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

#include "buffer.h"


class ax25_address
{
private:
	bool        valid    { false };
	std::string address;
	char        ssid     { '0'   };
	bool        end_mark { false };
	bool        repeated { false };

public:
	ax25_address();

	ax25_address(const std::vector<uint8_t> & from);

	ax25_address(const ax25_address & a);

	ax25_address(const std::string & a, const char ssid, const bool end_mark, const bool repeated);

	ax25_address & operator=(const ax25_address &);

	bool get_valid()    const { return valid;    }

	bool get_end_mark() const { return end_mark; }

	bool get_repeated() const { return repeated; }

	std::string get_address() const { return address;  }

	char        get_ssid() const    { return ssid;     }

	void set_address(const std::string & address, const char ssid);

	std::pair<uint8_t *, size_t> generate_address() const;
};

class ax25
{
private:
	bool                      valid    { false };
	ax25_address              from;
	ax25_address              to;
	std::vector<ax25_address> seen_by;
	uint8_t                   control  { 0     };
	std::optional<uint8_t>    msg_nr   {       };
	std::optional<uint8_t>    pid      {       };
	buffer                    data;

public:
	ax25();
	ax25(const std::vector<uint8_t> & in);
	~ax25();

	void set_from   (const std::string & callsign, const char ssid, const bool end_mark, const bool repeated);
	void set_to     (const std::string & callsign, const char ssid, const bool end_mark, const bool repeated);
	void set_control(const uint8_t control);
	void set_pid    (const uint8_t pid    );
	void set_data   (const uint8_t *const p, const size_t size);

	ax25_address get_from() const;
	ax25_address get_to  () const;
	std::vector<ax25_address> get_seen_by() const;
	buffer       get_data() const;
	std::optional<uint8_t> get_pid () const;
	bool         get_valid() const { return valid; }

	std::pair<uint8_t *, size_t> generate_packet() const;
};
