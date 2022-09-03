This gateway software can function as a gateway and digipeater for AX.25
and APRS over LoRa, axudp, local ax.25 network interface.

Packets can be stored in a MongoDB, viewed via a web-interface (using
web-sockets). The program has a built-in SNMP server (for statistics).
APRS data can be transmitted to APRS-SI. It can bridge between AXUDP,
local interface and LoRa: each bridge can have a filter with one or more
rules.

software requirements:
* cmake
* libax25-dev
* libconfig++-dev
* libjansson-dev
* libmicrohttpd-dev
* libmongocxx-dev      or from https://mongocxx.org/mongocxx-v3/installation/
* libmosquitto-dev
* libpigpio-dev
* libwebsockets-dev

optional hardware:
* SX1278 tranceiver (e.g. https://www.otronic.nl/a-65481848/lora/sx1278-ra-02-lora-module-433mhz/ )
* something with SPI and GPIO pins to connect the SX1278 to (e.g. a raspberry pi)


compiling program:
* mkdir build
* cd build
* cmake ..
* make
* cd ..

The result is: build/lora-aprs-gw


(optional) database schema:

```
  CREATE TABLE traffic (
    ts DATETIME NOT NULL,
    raw BLOB NOT NULL,
    other JSON NOT NULL,
    PRIMARY KEY(ts)
  );
```

Configuration: gateway.cfg 


This gateway software is using the LoRa SX1278 library
written by Yandiev Ruslan <yandievruslan@gmail.com>.
The original files are in the history of this git repository.

Folkert van Heusden <mail@vanheusden.com>
