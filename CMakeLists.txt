# (C) 2020-2022 by folkert van heusden <mail@vanheusden.com>, released under Apache License v2.0
cmake_minimum_required(VERSION 3.9.4)

project(lora-aprs-gw VERSION 6.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions("-D_FILE_OFFSET_BITS=64")

set(CMAKE_BUILD_TYPE Debug)

add_compile_options(-Wall -pedantic)

add_executable(lora-aprs-gw
	aprs-si.cpp
	db.cpp
	error.cpp
	gateway.cpp
	gps.cpp
	kiss.cpp
	log.cpp
	net.cpp
	snmp-data.cpp
	snmp-elem.cpp
	snmp.cpp
	stats.cpp
	utils.cpp
	webserver.cpp
	websockets.cpp
	LoRa.c
	)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
target_link_libraries(lora-aprs-gw Threads::Threads)

target_link_libraries(lora-aprs-gw -lrt)

include(FindPkgConfig)

pkg_check_modules(INIH REQUIRED inih)
target_link_libraries(lora-aprs-gw ${INIH_LIBRARIES})
target_include_directories(lora-aprs-gw PUBLIC ${INIH_INCLUDE_DIRS})
target_compile_options(lora-aprs-gw PUBLIC ${INIH_CFLAGS_OTHER})

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

target_link_libraries(lora-aprs-gw -lax25 -lmysqlcppconn -lpigpio -lutil)