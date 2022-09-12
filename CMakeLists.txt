# (C) 2020-2022 by folkert van heusden <mail@vanheusden.com>, released under Apache License v2.0
cmake_minimum_required(VERSION 3.9.4)

project(ham-router VERSION 0.3)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions("-D_FILE_OFFSET_BITS=64")

set(CMAKE_BUILD_TYPE Debug)

add_compile_options(-Wall -pedantic)

add_executable(ham-router
	ax25.cpp
	base64.cpp
	buffer.cpp
	configuration.cpp
	crc_32.c
	crc_ppp.cpp
	db.cpp
	db-common.cpp
	db-mongodb.cpp
	dissect-packet.cpp
	error.cpp
	filter.cpp
	gps.cpp
	hashing.cpp
	log.cpp
	LoRa.c
	main.cpp
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
	tranceiver-kiss-kernel.cpp
	tranceiver-kiss-tty.cpp
	tranceiver-lora-sx1278.cpp
	tranceiver-mqtt.cpp
	tranceiver-ws.cpp
	utils.cpp
	webserver.cpp
	websockets.cpp
	)

add_executable(test-dissect
	test-dissect.cpp
	dissect-packet.cpp
	db-common.cpp
	ax25.cpp
	buffer.cpp
	utils.cpp
	str.cpp
	net.cpp
	gps.cpp
	error.cpp
	log.cpp
	time.cpp
	)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
target_link_libraries(ham-router Threads::Threads)

target_link_libraries(test-dissect Threads::Threads)

target_link_libraries(ham-router -lrt)

if (EXISTS "/usr/include/pigpio.h")
	target_link_libraries(ham-router -lax25 -lpigpio -lutil)
	add_definitions(-DHAS_GPIO)
else()
	target_link_libraries(ham-router -lax25 -lutil)
endif()

if (NOT EXISTS "/usr/include/netax25/axlib.h")
	message(SEND_ERROR "libax25-dev is not installed")
endif()

target_link_libraries(test-dissect -lax25 -lutil -lgps -lconfig++)

include(FindPkgConfig)

pkg_check_modules(LIBMONGOCXX libmongocxx)
target_link_libraries(ham-router ${LIBMONGOCXX_LIBRARIES})
target_include_directories(ham-router PUBLIC ${LIBMONGOCXX_INCLUDE_DIRS})
target_compile_options(ham-router PUBLIC ${LIBMONGOCXX_CFLAGS_OTHER})

pkg_check_modules(CFG REQUIRED libconfig++)
target_link_libraries(ham-router ${CFG_LIBRARIES})
target_include_directories(ham-router PUBLIC ${CFG_INCLUDE_DIRS})
target_compile_options(ham-router PUBLIC ${CFG_CFLAGS_OTHER})

pkg_check_modules(JANSSON REQUIRED jansson)
target_link_libraries(ham-router ${JANSSON_LIBRARIES})
target_include_directories(ham-router PUBLIC ${JANSSON_INCLUDE_DIRS})
target_compile_options(ham-router PUBLIC ${JANSSON_CFLAGS_OTHER})

pkg_check_modules(HTTP libmicrohttpd)
target_link_libraries(ham-router ${HTTP_LIBRARIES})
target_include_directories(ham-router PUBLIC ${HTTP_INCLUDE_DIRS})
target_compile_options(ham-router PUBLIC ${HTTP_CFLAGS_OTHER})

pkg_check_modules(MOSQUITTO libmosquitto)
target_link_libraries(ham-router ${MOSQUITTO_LIBRARIES})
target_include_directories(ham-router PUBLIC ${MOSQUITTO_INCLUDE_DIRS})
target_compile_options(ham-router PUBLIC ${MOSQUITTO_CFLAGS_OTHER})

pkg_check_modules(WEBSOCKETS libwebsockets)
target_link_libraries(ham-router ${WEBSOCKETS_LIBRARIES})
target_include_directories(ham-router PUBLIC ${WEBSOCKETS_INCLUDE_DIRS})
target_compile_options(ham-router PUBLIC ${WEBSOCKETS_CFLAGS_OTHER})

pkg_check_modules(GPS libgps)
target_link_libraries(ham-router ${GPS_LIBRARIES})
target_include_directories(ham-router PUBLIC ${GPS_INCLUDE_DIRS})
target_compile_options(ham-router PUBLIC ${GPS_CFLAGS_OTHER})

configure_file(config.h.in config.h)
target_include_directories(ham-router PUBLIC "${PROJECT_BINARY_DIR}")
target_include_directories(test-dissect PUBLIC "${PROJECT_BINARY_DIR}")
