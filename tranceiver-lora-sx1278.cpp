#include <assert.h>
#include <errno.h>
#include <optional>
#include <string>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "log.h"
#include "net.h"
#include "random.h"
#include "str.h"
#include "time.h"
#include "tranceiver-lora-sx1278.h"
#include "utils.h"


void * rx_f(void *in)
{
	rxData *rx = reinterpret_cast<rxData *>(in);

	tranceiver_lora_sx1278 *const t = reinterpret_cast<tranceiver_lora_sx1278 *>(rx->userPtr);

	if (rx->size == 0 || rx->CRC) {
		t->count_packets(false);

		free(rx);

		return nullptr;
	}

	t->count_packets(true);

	uint64_t msg_id = get_random_uint64_t();

	message m(rx->last_time,
			myformat("LoRa-sx1278(%s)", t->get_id().c_str()),
			msg_id,
			reinterpret_cast<uint8_t *>(rx->buf),
			rx->size);

	std::map<std::string, db_record_data> meta;

	meta.insert({ "rssi", db_record_gen(myformat("%ddBm", rx->RSSI)) });

	meta.insert({ "air-time", db_record_gen(double(rx->Tpkt)) });

	m.set_meta(meta);

	t->mlog(LL_DEBUG, m, "rx_f", myformat("LoRa-sx1278, SNR: %f, RSSI: %d, CRC: %d: %s (ascii)", rx->SNR, rx->RSSI, rx->CRC, dump_replace(reinterpret_cast<uint8_t *>(rx->buf), rx->size).c_str()));

	t->queue_incoming_message(m);

	if (reinterpret_cast<tranceiver_lora_sx1278 *>(t)->is_digipeater()) {
		auto rc = t->put_message(m);

		if (rc != TE_ok)
			t->mlog(LL_WARNING, m, "rx_f", myformat("digipeat failed: %d", rc));
	}

	free(rx);

	return nullptr;
}

transmit_error_t tranceiver_lora_sx1278::put_message_low(const message & m)
{
#ifdef HAS_GPIO
	auto content = m.get_content();

	if (content.second > 255) {
		mlog(LL_WARNING, m, "put_message_low", myformat("packet too big (%d bytes)", content.second));

		return TE_hardware;
	}

	mlog(LL_DEBUG, m, "put_message_low", dump_replace(reinterpret_cast<const uint8_t *>(content.first), content.second));

	std::unique_lock<std::mutex> lck(lock);

	memcpy(modem.tx.data.buf, content.first, content.second);

	modem.tx.data.size = content.second;

	LoRa_stop_receive(&modem);  // manually stoping RxCont mode

	uint64_t start_ts = get_us();
	bool     fail     = false;

	while(LoRa_get_op_mode(&modem) != STDBY_MODE && !terminate && get_us() - start_ts < 60000000 /* timeout of 60s */)
		usleep(101000);

	if (LoRa_get_op_mode(&modem) != STDBY_MODE)
		fail = true;
	else {
		LoRa_send(&modem);

		while(LoRa_get_op_mode(&modem) != STDBY_MODE && !terminate && get_us() - start_ts < 60000000 /* timeout of 60s */)
			usleep(101000);

		if (LoRa_get_op_mode(&modem) != STDBY_MODE)
			fail = true;

		if (!fail) 
			mlog(LL_DEBUG, m, "put_message_low", myformat("time on air data - Tsym: %f; Tpkt: %f; payloadSymbNb: %u", modem.tx.data.Tsym, modem.tx.data.Tpkt, modem.tx.data.payloadSymbNb));
	}

	LoRa_receive(&modem);

	if (fail)
		log(LL_WARNING, "SX1278 is unresponsive");

	return fail ? TE_hardware : TE_ok;
#else
	return TE_hardware;
#endif
}

tranceiver_lora_sx1278::tranceiver_lora_sx1278(const std::string & id, seen *const s, work_queue_t *const w, gps_connector *const gps, const int dio0_pin, const int reset_pin, const bool digipeater, stats *const st, const size_t dev_nr) :
	tranceiver(id, s, w, gps),
	digipeater(digipeater)
{
	log(LL_INFO, "Instantiated LoRa SX1278");

	memset(&modem, 0x00, sizeof modem);

	valid_pkts   = st->register_stat(myformat("%s-valid-packets",   get_id().c_str()), myformat("1.3.6.1.2.1.4.57850.2.5.%zu.1", dev_nr), snmp_integer::si_counter64);

	invalid_pkts = st->register_stat(myformat("%s-invalid-packets", get_id().c_str()), myformat("1.3.6.1.2.1.4.57850.2.5.%zu.2", dev_nr), snmp_integer::si_counter64);

	// these settings are specific for APRS over LoRa

	modem.spiCS = 0;                         // Raspberry SPI CE pin number
	modem.rx.callback = rx_f;
	modem.rx.data.userPtr  = this;
	modem.tx.callback = nullptr;
	modem.tx.data.userPtr  = this;
	modem.eth.preambleLen = 8;
	modem.eth.bw = BW125;                    // Bandwidth 125KHz
	modem.eth.sf = SF12;                     // Spreading Factor 12
	modem.eth.ecr = CR5;                     // Error coding rate CR4/8
	modem.eth.freq = 433775000;              // 434.8MHz
	modem.eth.resetGpioN = reset_pin;        // tranceiver reset pin
	modem.eth.dio0GpioN = dio0_pin;
	modem.eth.outPower = OP20;               // Output power
	modem.eth.powerOutPin = PA_BOOST;        // Power Amplifire pin
	modem.eth.AGC = 1;                       // enable Auto Gain Control
	modem.eth.OCP = 240;                     // 45 to 240 mA. 0 to turn off protection
	modem.eth.implicitHeader = 0;            // select "explicit header" mode
	modem.eth.syncWord = 0x12;
	modem.eth.CRC = 1;

	LoRa_begin(&modem);

	LoRa_receive(&modem);
}

tranceiver_lora_sx1278::~tranceiver_lora_sx1278()
{
	LoRa_end(&modem);
}

void tranceiver_lora_sx1278::operator()()
{
}

void tranceiver_lora_sx1278::count_packets(const bool valid)
{
	if (valid)
		stats_inc_counter(valid_pkts);
	else
		stats_inc_counter(invalid_pkts);
}

tranceiver *tranceiver_lora_sx1278::instantiate(const libconfig::Setting & node_in, work_queue_t *const w, gps_connector *const gps, stats *const st, const size_t dev_nr)
{
	std::string  id;
	seen        *s          = nullptr;
	int          dio0_pin   = -1;
	int          reset_pin  = -1;
	bool         digipeater = false;

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "repetition-rate-limiting") {
			if (s)
				error_exit(true, "(line %d): repetition-rate-limiting already defined", node.getSourceLine());

			s = seen::instantiate(node);
		}
		else if (type == "dio0-pin")
			dio0_pin = node_in.lookup(type);
		else if (type == "reset-pin")
			reset_pin = node_in.lookup(type);
		else if (type == "digipeater")
			digipeater = node_in.lookup(type);
		else if (type != "type") {
			error_exit(false, "(line %d): setting \"%s\" is not known", node.getSourceLine(), type.c_str());
		}
        }

	return new tranceiver_lora_sx1278(id, s, w, gps, dio0_pin, reset_pin, digipeater, st, dev_nr);
}
