This gateway software is using the LoRa SX1278 library
written by Yandiev Ruslan <yandievruslan@gmail.com>.
The original files are in the history of this git repository.

requires:
* libax25-dev
* libinih-dev


(optional) database schema:

CREATE TABLE `APRS` (
  `ts` datetime NOT NULL,
  `rssi` double DEFAULT NULL,
  `snr` double DEFAULT NULL,
  `crc` int(1) NOT NULL,
  `content` blob DEFAULT NULL,
  `latitude` double DEFAULT NULL,
  `longitude` double DEFAULT NULL,
  `distance` double DEFAULT NULL
);


Folkert van Heusden <mail@vanheusden.com>
