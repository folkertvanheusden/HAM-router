This gateway software can receive APRS and AX.25 over LoRa.
It can also send AX.25 over LoRa.

Packets can be logged as json via MQTT, WebSockets and syslog. They can
also be stored in a (MySQL compatible-)database. Statistics of this
server-program can be retreived via SNMP. APRS data can also be send to
APRS-IS. It also has a simple web-interface.

software requirements:
* cmake
* libax25-dev
* libinih-dev
* libjansson-dev
* libmicrohttpd-dev
* libmosquitto-dev
* libmysqlcppconn-dev
* libpigpio-dev
* libwebsockets-dev

hardware requirements:
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
  CREATE TABLE APRS (
    ts datetime NOT NULL,
    rssi double DEFAULT NULL,
    snr double DEFAULT NULL,
    crc int(1) NOT NULL,
    content blob DEFAULT NULL,
    latitude double DEFAULT NULL,
    longitude double DEFAULT NULL,
    distance double DEFAULT NULL
  );
```

Configuration: gateway.ini 


This gateway software is using the LoRa SX1278 library
written by Yandiev Ruslan <yandievruslan@gmail.com>.
The original files are in the history of this git repository.

Folkert van Heusden <mail@vanheusden.com>
