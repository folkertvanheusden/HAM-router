// (C) 2021-2022 by folkert van heusden <mail@vanheusden.com>, released under Apache License v2.0
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "error.h"
#include "log.h"
#include "snmp.h"
#include "snmp-elem.h"
#include "str.h"
#include "utils.h"


snmp::snmp(snmp_data *const sd, stats *const s, const int port) :
	sd(sd),
	s(s),
	port(port)
{
	th = new std::thread(std::ref(*this));
}

snmp::~snmp()
{
}

uint64_t snmp::get_INTEGER(const uint8_t *p, const size_t length)
{
	uint64_t v = 0;

	if (length > 8)
		log(LL_DEBUG_VERBOSE, "SNMP: INTEGER truncated (%zu bytes)", length);

	for(size_t i=0; i<length; i++) {
		v <<= 8;
		v |= *p++;
	}

	return v;
}

bool snmp::get_type_length(const uint8_t *p, const size_t len, uint8_t *const type, uint8_t *const length)
{
	if (len < 2)
		return false;

	*type = *p++;

	*length = *p++;

	return true;
}

bool snmp::get_OID(const uint8_t *p, const size_t length, std::string *const oid_out)
{
	oid_out->clear();

	uint32_t v = 0;

	for(size_t i=0; i<length; i++) {
		if (p[i] < 128) {
			v <<= 7;
			v |= p[i];

			if (i == 0 && v == 43)
				*oid_out += "1.3";
			else
				*oid_out += myformat(".%d", v);

			v = 0;
		}
		else {
			v <<= 7;
			v |= p[i] & 127;
		}
	}

	if (v) {
		log(LL_DEBUG_VERBOSE, "SNMP: object identifier did not properly terminate");
		return false;
	}

	return true;
}

bool snmp::process_PDU(const uint8_t *p, const size_t len, oid_req_t *const oids_req, const bool is_getnext)
{
	uint8_t pdu_type = 0, pdu_length = 0;

	// ID
	if (!get_type_length(p, len, &pdu_type, &pdu_length))
		return false;

	if (pdu_type != 0x02) // expecting an integer here)
		return false;

	p += 2;

	oids_req->req_id = get_INTEGER(p, pdu_length);
	p += pdu_length;

	// error
	if (!get_type_length(p, len, &pdu_type, &pdu_length))
		return false;

	if (pdu_type != 0x02) // expecting an integer here)
		return false;

	p += 2;

	uint64_t error = get_INTEGER(p, pdu_length);
	(void)error;
	p += pdu_length;

	// error index
	if (!get_type_length(p, len, &pdu_type, &pdu_length))
		return false;

	if (pdu_type != 0x02) // expecting an integer here)
		return false;

	p += 2;

	uint64_t error_index = get_INTEGER(p, pdu_length);
	(void)error_index;
	p += pdu_length;

	// varbind list sequence
	uint8_t type_vb_list = *p++;
	if (type_vb_list != 0x30)
		return false;
	uint8_t len_vb_list = *p++;

	const uint8_t *pnt = p;

	while(pnt < &p[len_vb_list]) {
		uint8_t seq_type = *pnt++;
		uint8_t seq_length = *pnt++;

		if (&pnt[seq_length] > &p[len_vb_list]) {
			log(LL_DEBUG_VERBOSE, "SNMP: length field out of bounds");
			return false;
		}

		if (seq_type == 0x30) {  // sequence
			process_BER(pnt, seq_length, oids_req, is_getnext, 0);
			pnt += seq_length;
		}
		else {
			log(LL_DEBUG_VERBOSE, "SNMP: unexpected/invalid type %02x", seq_type);
			return false;
		}
	}

	return true;
}

bool snmp::process_BER(const uint8_t *p, const size_t len, oid_req_t *const oids_req, const bool is_getnext, const int is_top)
{
	if (len < 2) {
		log(LL_DEBUG_VERBOSE, "SNMP: BER too small");
		return false;
	}

	const uint8_t *pnt = p;
	bool first_integer = true;
	bool first_octet_str = true;

	while(pnt < &p[len]) {
		uint8_t type = *pnt++;
		uint8_t length = *pnt++;

		if (&pnt[length] > &p[len]) {
			log(LL_DEBUG_VERBOSE, "SNMP: length field out of bounds");
			return false;
		}

		if (type == 0x02) {  // integer
			if (is_top && first_integer)
				oids_req->version = get_INTEGER(pnt, length);

			first_integer = false;

			pnt += length;
		}
		else if (type == 0x04) {  // octet string
			std::string v((const char *)pnt, length);

			if (is_top && first_octet_str)
				oids_req->community = v;

			first_octet_str = false;

			pnt += length;
		}
		else if (type == 0x05) {  // null
			// ignore for now
			pnt += length;
		}
		else if (type == 0x06) {  // object identifier
			std::string oid_out;

			if (!get_OID(pnt, length, &oid_out))
				return false;

			if (is_getnext) {
				std::string oid_next = sd->find_next_oid(oid_out);

				if (oid_next.empty()) {
					oids_req->err = 2;
					oids_req->err_idx = 1;
				}
				else {
					oids_req->oids.push_back(oid_next);
				}
			}
			else {
				oids_req->oids.push_back(oid_out);
			}

			pnt += length;
		}
		else if (type == 0x30) {  // sequence
			if (!process_BER(pnt, length, oids_req, is_getnext, is_top - 1))
				return false;

			pnt += length;
		}
		else if (type == 0xa0) {  // GetRequest PDU
			if (!process_PDU(pnt, length, oids_req, is_getnext))
				return false;
			pnt += length;
		}
		else if (type == 0xa1) {  // GetNextRequest PDU
			if (!process_PDU(pnt, length, oids_req, true))
				return false;
			pnt += length;
		}
		else if (type == 0xa3) {  // SetRequest PDU
			if (!process_PDU(pnt, length, oids_req, is_getnext))
				return false;
			pnt += length;
		}
		else {
			log(LL_DEBUG_VERBOSE, "SNMP: invalid type %02x", type);
			return false;
		}
	}

	return true;
}

void snmp::gen_reply(oid_req_t & oids_req, uint8_t **const packet_out, size_t *const output_size)
{
	snmp_sequence *se = new snmp_sequence();

	se->add(new snmp_integer(snmp_integer::si_integer, oids_req.version));  // version

	std::string community = oids_req.community;
	if (community.empty())
		community = "public";

	se->add(new snmp_octet_string((const uint8_t *)community.c_str(), community.size()));  // community string

	// request pdu
	snmp_pdu *GetResponsePDU = new snmp_pdu(0xa2);
	se->add(GetResponsePDU);

	GetResponsePDU->add(new snmp_integer(snmp_integer::si_integer, oids_req.req_id));  // ID

	GetResponsePDU->add(new snmp_integer(snmp_integer::si_integer, oids_req.err));  // error

	GetResponsePDU->add(new snmp_integer(snmp_integer::si_integer, oids_req.err_idx));  // error index

	snmp_sequence *varbind_list = new snmp_sequence();
	GetResponsePDU->add(varbind_list);

	for(auto e : oids_req.oids) {
		snmp_sequence *varbind = new snmp_sequence();
		varbind_list->add(varbind);

		varbind->add(new snmp_oid(e));

		log(LL_DEBUG_VERBOSE, "SNMP requested: %s", e.c_str());

		std::optional<snmp_elem *> rc = sd->find_by_oid(e);

		std::size_t dot       = e.rfind('.');
		std::string ends_with = dot != std::string::npos ? e.substr(dot) : "";

		if (!rc.has_value() && ends_with == ".0")
			rc = sd->find_by_oid(e.substr(0, dot));

		if (rc.has_value()) {
			auto current_element = rc.value();

			if (current_element)
				varbind->add(current_element);
			else
				varbind->add(new snmp_null());
		}
		else {
			log(LL_DEBUG_VERBOSE, "SNMP: requested %s not found, returning null", e.c_str());

			// FIXME snmp_null?
			varbind->add(new snmp_null());
		}
	}

	auto rc = se->get_payload();
	*packet_out = rc.first;
	*output_size = rc.second;

	delete se;
}

void snmp::input(const int fd, const uint8_t *const data, const size_t data_len, const sockaddr *const a, const size_t a_len)
{
//	log(LL_DEBUG_VERBOSE, "SNMP: request from [%s]:%d", src_ip.to_str().c_str(), src_port);

	oid_req_t or_;

	if (!process_BER(data, data_len, &or_, false, 2)) {
                log(LL_DEBUG_VERBOSE, "SNMP: failed processing request");
                return;
	}

	uint8_t *packet_out = nullptr;
	size_t output_size = 0;

	gen_reply(or_, &packet_out, &output_size);

	if (output_size) {
		// log(LL_DEBUG_VERBOSE, "SNMP: sending reply of %zu bytes to [%s]:%d", output_size, src_ip.to_str().c_str(), src_port);

		sendto(fd, packet_out, output_size, 0, a, a_len);

		free(packet_out);
	}
}

void snmp::operator()()
{
	set_thread_name("snmp");

	if (port == -1)
		return;

	log(LL_INFO, "Starting SNMP server");

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1)
		error_exit(true, "socket() failed");

	struct sockaddr_in servaddr { 0 };

	servaddr.sin_family      = AF_INET; // IPv4
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port        = htons(port);

	if (bind(fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
		error_exit(true, "bind() failed");

	for(;;) {
		try {
			char               buffer[1600] { 0 };
			struct sockaddr_in clientaddr   { 0 };
			socklen_t          len = sizeof(clientaddr);

			int n = recvfrom(fd, buffer, sizeof buffer, 0, (sockaddr *)&clientaddr, &len);

			if (n)
				input(fd, reinterpret_cast<uint8_t *>(buffer), n, (const sockaddr *)&clientaddr, len);
                }
                catch(const std::exception& e) {
                        log(LL_ERR, "snmp::operator(): exception %s", e.what());
                }
	}
}
