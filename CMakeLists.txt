cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)
include(FindPkgConfig)

project (wavedcc)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(ignoreMe "${USE_PIGPIOD_IF}")

find_package(Threads REQUIRED)
find_package(pigpio)

add_library(dccpacket OBJECT dccpacket.cpp)
add_library(dccengine OBJECT dccengine.cpp)

add_executable(wavedcc wavedcc.cpp)
add_executable(wavedccd wavedccd.cpp)

if (DEFINED CACHE{USE_PIGPIOD_IF})

set(CMAKE_CXX_FLAGS "-DUSE_PIGPIOD_IF")
target_include_directories(wavedcc SYSTEM PUBLIC ${PIGPIO_INCLUDE_DIR})
target_link_libraries(wavedcc dccengine dccpacket Threads::Threads ${pigpiod_if2_LIBRARY})
target_include_directories(wavedccd SYSTEM PUBLIC ${PIGPIO_INCLUDE_DIR})
target_link_libraries(wavedccd dccengine dccpacket Threads::Threads ${pigpiod_if2_LIBRARY})

else()

target_include_directories(wavedcc PRIVATE ${pigpio_INCLUDE_DIR} )
target_link_libraries(wavedcc dccengine dccpacket Threads::Threads ${pigpio_LIBRARY})
target_include_directories(wavedccd PRIVATE ${pigpio_INCLUDE_DIRS} )
target_link_libraries(wavedccd dccengine dccpacket Threads::Threads ${pigpio_LIBRARY})

endif()
