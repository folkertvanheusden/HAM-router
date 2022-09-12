#include "ax25.h"
#include "dissect-packet.h"
#include "gps.h"
#include "log.h"
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

	if ((data[14] & 3) == 1)  // non existing control-byte field
		return { };

	// may be ax.25:

	ax25 packet(std::vector<uint8_t>(data, &data[len]));

	std::map<std::string, db_record_data> fields;

	fields.insert({ "protocol", db_record_gen("AX.25") });

	fields.insert({ "from", db_record_gen(packet.get_from().get_address()) });

	fields.insert({ "to",   db_record_gen(packet.get_to  ().get_address()) });

	buffer payload = packet.get_data();

	fields.insert({ "payload",  db_record_gen(dump_replace(payload.get_pointer(), payload.get_size())) });

	auto pid = packet.get_pid();

	if (pid.has_value()) {
		switch(pid.value()) {
			case 0x01:  // ISO 8208/CCITT X.25 PLP
				fields.insert({ "protocol", db_record_gen("X.25") }); break;
			case 0x06:  // Compressed TCP/IP packet
				fields.insert({ "protocol", db_record_gen("compressed TCP/IP") }); break;
			case 0x07:  // Uncompressed TCP/IP packet
				fields.insert({ "protocol", db_record_gen("uncompressed TCP/IP") }); break;
			case 0x08:  // Segmentation fragment
				fields.insert({ "protocol", db_record_gen("segmentation fragment") }); break;	
			case 0xc3:  // Text Telephone
				fields.insert({ "protocol", db_record_gen("TEXNET") }); break;
			case 0xc4:  // Link Quality Protocol
				fields.insert({ "protocol", db_record_gen("LQP") }); break;
			case 0xca:  // Appletalk
				fields.insert({ "protocol", db_record_gen("Appletalk") }); break;
			case 0xcb:  // Appletalk ARP
				fields.insert({ "protocol", db_record_gen("Appletalk ARP") }); break;
			case 0xcc:  // ARPA Internet Protocol
				fields.insert({ "protocol", db_record_gen("IP") }); break;
			case 0xcd:  // ARPA Address Resolution Protocol
				fields.insert({ "protocol", db_record_gen("ARP") }); break;
			case 0xce:  // FlexNet
				fields.insert({ "protocol", db_record_gen("FlexNet") }); break;
			case 0xcf:  // NET/ROM
				fields.insert({ "protocol", db_record_gen("NET/ROM") }); break;
			case 0xf0:  // no layer 3
				fields.insert({ "TEXT", db_record_gen("NMEA") }); break;
			case 0xff:  // next byte contains more info
				log(LL_WARNING, "AX.25: \"next byte contains more info\" - UNHANDLED");
				break;
		}
	}

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

	if (colon == std::string::npos)
		return { };

	std::size_t chars_left = work.size() - colon;

	if (chars_left == 0)
		return { };

	char command = work[colon + 1];

	if ((command == '!' || command == '=') && chars_left >= 19) {
		std::string nmea = work.substr(colon + 1);

		auto position = parse_nmea_pos(nmea.c_str());

		if (position.has_value()) {
			fields.insert({ "latitude",  db_record_gen(position.value().first)  });

			fields.insert({ "longitude", db_record_gen(position.value().second) });
		}
	}
	else if (command == '$') {
		fields.insert({ "payload",   db_record_gen(work.substr(colon + 2)) });

		fields.insert({ "payload-protocol", db_record_gen("NMEA") });
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
