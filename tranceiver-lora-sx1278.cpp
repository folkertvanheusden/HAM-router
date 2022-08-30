#include <errno.h>
#include <optional>
#include <string>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "log.h"
#include "net.h"
#include "str.h"
#include "tranceiver-lora-sx1278.h"
#include "utils.h"


void * rx_f(void *in)
{
	rxData *rx = reinterpret_cast<rxData *>(in);
	if (rx->size == 0) {
		free(rx);

		return nullptr;
	}

	log(LL_DEBUG, "LoRa-sx1278 rx: %s", dump_replace(reinterpret_cast<uint8_t *>(rx->buf), rx->size).c_str());

	tranceiver *const t = reinterpret_cast<tranceiver_lora_sx1278 *>(rx->userPtr);

	message_t m;
	m.tv       = rx->last_time;
	m.source   = myformat("LoRa-sx1278(%s)", t->get_id().c_str());
	m.message  = reinterpret_cast<uint8_t *>(duplicate(rx->buf, rx->size));
	m.s        = rx->size;
	m.from_rf  = true;
	m.air_time = rx->at.Tpkt;

	if (t->queue_incoming_message(m) != TE_ok)
		free(m.message);

	free(rx);

	return nullptr;
}

transmit_error_t tranceiver_lora_sx1278::put_message_low(const uint8_t *const p, const size_t len)
{
#ifdef HAS_GPIO
	if (len > 255) {
		log(LL_WARNING, "tranceiver_lora_sx1278::put_message_low: packet too big (%d bytes)", len);

		return TE_hardware;
	}

	std::unique_lock<std::mutex> lck(lock);

	log(LL_DEBUG, "tranceiver_lora_sx1278::put_message_low: %s", dump_replace(reinterpret_cast<const uint8_t *>(modem.tx.data.buf), len).c_str());

	memcpy(modem.tx.data.buf, p, len);

	modem.tx.data.size = len;

	LoRa_stop_receive(&modem);  // manually stoping RxCont mode

	// TODO: timeout
	while(LoRa_get_op_mode(&modem) != STDBY_MODE)
		usleep(101000);

	LoRa_send(&modem);

	// TODO: timeout
	while(LoRa_get_op_mode(&modem) != STDBY_MODE)
		usleep(101000);

	log(LL_DEBUG, "tranceiver_lora_sx1278::put_message_low: time on air data - Tsym: %f; Tpkt: %f; payloadSymbNb: %u", modem.tx.data.at.Tsym, modem.tx.data.at.Tpkt, modem.tx.data.at.payloadSymbNb);
	// TODO: calculate overhead by measuring how long this routine took

	LoRa_receive(&modem);

	return TE_ok;
#else
	return TE_hardware;
#endif
}

tranceiver_lora_sx1278::tranceiver_lora_sx1278(const std::string & id, seen *const s, work_queue_t *const w, const int dio0_pin, const int reset_pin) :
	tranceiver(id, s, w)
{
	log(LL_INFO, "Instantiated LoRa SX1278 (%s)", id.c_str());

	memset(&modem, 0x00, sizeof modem);

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

	log(LL_INFO, "LoRa SX1278 (%s) initialized", id.c_str());
}

tranceiver_lora_sx1278::~tranceiver_lora_sx1278()
{
	LoRa_end(&modem);
}

void tranceiver_lora_sx1278::operator()()
{
}

tranceiver *tranceiver_lora_sx1278::instantiate(const libconfig::Setting & node_in, work_queue_t *const w)
{
	std::string  id;
	seen        *s = nullptr;
	int          dio0_pin  = -1;
	int          reset_pin = -1;

        for(int i=0; i<node_in.getLength(); i++) {
                const libconfig::Setting & node = node_in[i];

		std::string type = node.getName();

		if (type == "id")
			id = node_in.lookup(type).c_str();
		else if (type == "incoming-rate-limiting")
			s = seen::instantiate(node);
		else if (type == "dio0-pin")
			dio0_pin = node_in.lookup(type);
		else if (type == "reset-pin")
			reset_pin = node_in.lookup(type);
		else if (type != "type")
			error_exit(false, "setting \"%s\" is now known", type.c_str());
        }

	return new tranceiver_lora_sx1278(id, s, w, dio0_pin, reset_pin);
}
