# (C) 2020-2022 by folkert van heusden <mail@vanheusden.com>, released under Apache License v2.0
cmake_minimum_required(VERSION 3.9.4)

project(lora-aprs-gw VERSION 6.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions("-D_FILE_OFFSET_BITS=64")

set(CMAKE_BUILD_TYPE Debug)

add_compile_options(-Wall -pedantic)

add_executable(lora-aprs-gw
	ax25.cpp
	buffer.cpp
	configuration.cpp
	crc_32.c
	crc_ppp.cpp
	db.cpp
	db-common.cpp
	db-mongodb.cpp
	dissect-packet.cpp
	error.cpp
	gateway.cpp
	gps.cpp
	hashing.cpp
	log.cpp
	LoRa.c
	message.cpp
	net.cpp
	random.cpp
	rate-limiter.cpp
	seen.cpp
	snmp-data.cpp
	snmp-elem.cpp
	snmp.cpp
	stats.cpp
	str.cpp
	switchboard.cpp
	time.cpp
	tranceiver.cpp
	tranceiver-aprs-si.cpp
	tranceiver-axudp.cpp
	tranceiver-beacon.cpp
	tranceiver-db.cpp
	tranceiver-kiss.cpp
	tranceiver-lora-sx1278.cpp
	tranceiver-mqtt.cpp
	utils.cpp
	webserver.cpp
	websockets.cpp
	)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
target_link_libraries(lora-aprs-gw Threads::Threads)

target_link_libraries(lora-aprs-gw -lrt)

if (EXISTS "/usr/include/pigpio.h")
	target_link_libraries(lora-aprs-gw -lax25 -lmysqlcppconn -lpigpio -lutil)
	add_definitions(-DHAS_GPIO)
else()
	target_link_libraries(lora-aprs-gw -lax25 -lmysqlcppconn -lutil)
endif()

include(FindPkgConfig)

pkg_check_modules(LIBMONGOCXX libmongocxx)
target_link_libraries(lora-aprs-gw ${LIBMONGOCXX_LIBRARIES})
target_include_directories(lora-aprs-gw PUBLIC ${LIBMONGOCXX_INCLUDE_DIRS})
target_compile_options(lora-aprs-gw PUBLIC ${LIBMONGOCXX_CFLAGS_OTHER})

pkg_check_modules(CFG REQUIRED libconfig++)
target_link_libraries(lora-aprs-gw ${CFG_LIBRARIES})
target_include_directories(lora-aprs-gw PUBLIC ${CFG_INCLUDE_DIRS})
target_compile_options(lora-aprs-gw PUBLIC ${CFG_CFLAGS_OTHER})

pkg_check_modules(JANSSON REQUIRED jansson)
target_link_libraries(lora-aprs-gw ${JANSSON_LIBRARIES})
target_include_directories(lora-aprs-gw PUBLIC ${JANSSON_INCLUDE_DIRS})
target_compile_options(lora-aprs-gw PUBLIC ${JANSSON_CFLAGS_OTHER})

pkg_check_modules(HTTP REQUIRED libmicrohttpd)
target_link_libraries(lora-aprs-gw ${HTTP_LIBRARIES})
target_include_directories(lora-aprs-gw PUBLIC ${HTTP_INCLUDE_DIRS})
target_compile_options(lora-aprs-gw PUBLIC ${HTTP_CFLAGS_OTHER})

pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
target_link_libraries(lora-aprs-gw ${MOSQUITTO_LIBRARIES})
target_include_directories(lora-aprs-gw PUBLIC ${MOSQUITTO_INCLUDE_DIRS})
target_compile_options(lora-aprs-gw PUBLIC ${MOSQUITTO_CFLAGS_OTHER})

pkg_check_modules(WEBSOCKETS REQUIRED libwebsockets)
target_link_libraries(lora-aprs-gw ${WEBSOCKETS_LIBRARIES})
target_include_directories(lora-aprs-gw PUBLIC ${WEBSOCKETS_INCLUDE_DIRS})
target_compile_options(lora-aprs-gw PUBLIC ${WEBSOCKETS_CFLAGS_OTHER})

configure_file(config.h.in config.h)
target_include_directories(lora-aprs-gw PUBLIC "${PROJECT_BINARY_DIR}")
