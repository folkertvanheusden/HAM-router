CFLAGS=-Wall -pedantic -ggdb3 -Ofast
CXXFLAGS=-Wall -pedantic -ggdb3 -Ofast -std=c++20

all: gateway

LoRa.o: LoRa.c
	gcc $(CFLAGS) -c LoRa.c -o LoRa.o -lpigpio -lrt -pthread -lm

gateway.o: gateway.cpp
	g++ $(CXXFLAGS) -c gateway.cpp -o gateway.o

db.o: db.cpp
	g++ $(CXXFLAGS) -c db.cpp -o db.o

error.o: error.cpp
	g++ $(CXXFLAGS) -c error.cpp -o error.o

log.o: log.cpp
	g++ $(CXXFLAGS) -c log.cpp -o log.o

utils.o: utils.cpp
	g++ $(CXXFLAGS) -c utils.cpp -o utils.o

net.o: net.cpp
	g++ $(CXXFLAGS) -c net.cpp -o net.o

kiss.o: kiss.cpp
	g++ $(CXXFLAGS) -c kiss.cpp -o kiss.o

websockets.o: websockets.cpp
	g++ $(CXXFLAGS) -c websockets.cpp -o websockets.o

gateway: gateway.o error.o db.o log.o utils.o net.o kiss.o websockets.o snmp-data.o snmp-elem.o snmp.o stats.o LoRa.o
	g++ $(CXXFLAGS) -o gateway gateway.o error.o LoRa.o log.o db.o utils.o net.o kiss.o websockets.o snmp-data.o snmp-elem.o snmp.o stats.o -lpigpio -lrt -pthread -lax25 -lutil -lm -lmysqlcppconn -linih -ljansson -lmosquitto -lwebsockets
