This software can function as a gateway, router, bridge and digipeater for
e.g. AX.25 and APRS over LoRa, axudp, local ax.25 network interfaces and
TNC devices (KISS over serial).

Packets can be stored in a MongoDB, viewed via a web-interface (using
web-sockets). The program has a built-in SNMP server (for statistics).
APRS data can be transmitted to APRS-SI. It can bridged and routed between
AXUDP, local interface, KISS interfaces and LoRa: each bridge can have a
filter with one or more rules.

software requirements:
* cmake
* libax25-dev
* libconfig++-dev
* libgps-dev           (optional)
* libjansson-dev
* libmicrohttpd-dev    (optional)
* libmongocxx-dev      or from https://mongocxx.org/mongocxx-v3/installation/  (optional)
* libmosquitto-dev     (optional)
* libpigpio-dev        (optional)
* libwebsockets-dev    (optional)

optional hardware:
* SX1278 tranceiver (e.g. https://www.otronic.nl/a-65481848/lora/sx1278-ra-02-lora-module-433mhz/ )
* something with SPI and GPIO pins to connect the SX1278 to (e.g. a raspberry pi)


compiling program:
* mkdir build
* cd build
* cmake ..
* make
* cd ..

The result is: build/ham-router


Configuration: ham-router.cfg 


This software is using the LoRa SX1278 library
written by Yandiev Ruslan <yandievruslan@gmail.com>.
The original files are in the history of this git repository.

Folkert van Heusden <mail@vanheusden.com>
