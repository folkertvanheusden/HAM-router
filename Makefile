CFLAGS=-Wall -pedantic -ggdb3 -Ofast
CXXFLAGS=-Wall -pedantic -ggdb3 -Ofast

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

gateway: LoRa.o gateway.o error.o db.o log.o utils.o
	g++ $(CXXFLAGS) -o gateway gateway.o error.o LoRa.o log.o db.o utils.o -lpigpio -lrt -pthread -lax25 -lutil -lm -lmysqlcppconn
