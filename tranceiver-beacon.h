#include <string>
#include <vector>

#include "tranceiver.h"


typedef enum { beacon_mode_ax25, beacon_mode_aprs } beacon_mode_t;

class tranceiver_beacon : public tranceiver
{
private:
	const std::string   beacon_text;
	const int           beacon_interval   { 300              };
	const beacon_mode_t bm                { beacon_mode_aprs };
	const std::string   callsign;
	const char          aprs_symbol_table { '/'              };
	const std::string   aprs_symbol       { "&L"             };

protected:
	transmit_error_t put_message_low(const message & m) override;

public:
	tranceiver_beacon(const std::string & id, seen *const s, work_queue_t *const w, gps_connector *const gps, const std::string & beacon_text, const int beacon_interval, const beacon_mode_t bm, const std::string & callsign, const char aprs_symbol_table, const std::string & aprs_symbol);
	virtual ~tranceiver_beacon();

	std::string get_type_name() const override { return "beacon"; }

	static tranceiver *instantiate(const libconfig::Setting & node, work_queue_t *const w, gps_connector *const gps);

	void operator()() override;
};
