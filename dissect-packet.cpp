#include "ax25.h"
#include "dissect-packet.h"
#include "gps.h"
#include "str.h"


std::optional<std::map<std::string, db_record_data> > parse_ax25(const uint8_t *const data, const size_t len)
{
	if (len < 16)
		return { };

	// verify if from address has not bit 0 set
	bool bit_0_set = false;
	for(int i=0; i<6; i++)
		bit_0_set |= data[i] & 0x01;

	if (bit_0_set)
		return { };

	// TODO: control-byte must not have '01' for lower 2 bits

	// may be ax.25:

	ax25 packet(std::vector<uint8_t>(data, &data[len]));

	std::map<std::string, db_record_data> fields;

	fields.insert({ "protocol", db_record_gen("AX.25") });

	fields.insert({ "from", db_record_gen(packet.get_from().get_address()) });

	fields.insert({ "to",   db_record_gen(packet.get_to  ().get_address()) });

	buffer payload = packet.get_data();

	fields.insert({ "payload",  db_record_gen(dump_replace(payload.get_pointer(), payload.get_size())) });

	return fields;
}

std::optional<std::map<std::string, db_record_data> > parse_aprs(const uint8_t *const data, const size_t len)
{
	if (len < 6)
		return { };

	if (data[0] != '<' || data[1] != 0xff || data[2] != 0x01)
		return { };

	// might be an APRS packet

	std::map<std::string, db_record_data> fields;

	const std::string work(reinterpret_cast<const char *>(data), len);

	// get address
        std::size_t gt = work.find('>');

        if (gt != std::string::npos) {
                std::string from;
                std::string to;

                std::size_t colon = work.find(':', gt);

                if (colon != std::string::npos) {
                        std::string to_full = work.substr(gt + 1, colon - gt - 1);

                        std::size_t delimiter = to_full.find(',');

                        if (delimiter != std::string::npos)
                                to = to_full.substr(0, delimiter);
                        else
                                to = to_full;

                        from    = work.substr(3, gt - 3);

			fields.insert({ "from", from });

			fields.insert({ "to",   to   });
		}
	}

	// get nmea position
        std::size_t colon = work.find(':');

	if (colon != std::string::npos && len - colon >= 8) {
		double latitude = 0, longitude = 0;

		std::string nmea = work.substr(colon + 1);

		parse_nmea_pos(nmea.c_str(), &latitude, &longitude);

		fields.insert({ "latitude",  db_record_gen(latitude)  });

		fields.insert({ "longitude", db_record_gen(longitude) });
	}

        std::size_t bracket = work.find('[');

	if (bracket != std::string::npos)
		fields.insert({ "payload",   db_record_gen(work.substr(bracket + 1)) });

	fields.insert({ "protocol", db_record_gen("APRS-OE") });

	return fields;
}

std::optional<std::map<std::string, db_record_data> > dissect_packet(const uint8_t *const data, const size_t len)
{
	auto aprs = parse_aprs(data, len);

	if (aprs.has_value())
		return aprs;

	auto ax25 = parse_ax25(data, len);

	if (ax25.has_value())
		return ax25;

	return { };
}
