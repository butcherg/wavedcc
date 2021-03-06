cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)
include(FindPkgConfig)

project (wavedcc)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(ignoreMe "${USE_PIGPIOD_IF}")

option(USE_PIGPIOD_IF "Enables GPIO interface through the pigpiod daemon" ON)
option(USE_PIGPIO "Enables direct GPIO interface, requires root" OFF)

find_package(Threads REQUIRED)
find_package(pigpio REQUIRED)

add_library(dccpacket OBJECT dccpacket.cpp)
add_library(dccengine OBJECT dccengine.cpp)
add_library(DatagramSocket OBJECT DatagramSocket.cpp)

add_executable(wavedcc wavedcc.cpp)
add_executable(wavedccd wavedccd.cpp)
add_executable(dcclog dcclog.cpp)

target_link_libraries(dcclog DatagramSocket)

if (USEPIGPIOD_IF)

set(CMAKE_CXX_FLAGS "-DUSE_PIGPIOD_IF")
target_include_directories(wavedcc SYSTEM PUBLIC ${PIGPIO_INCLUDE_DIR})
target_link_libraries(wavedcc dccengine dccpacket DatagramSocket Threads::Threads ${pigpiod_if2_LIBRARY})
target_include_directories(wavedccd SYSTEM PUBLIC ${PIGPIO_INCLUDE_DIR})
target_link_libraries(wavedccd dccengine dccpacket DatagramSocket Threads::Threads ${pigpiod_if2_LIBRARY})

elseif (USE_PIGPIO)

target_include_directories(wavedcc PRIVATE ${pigpio_INCLUDE_DIR} )
target_link_libraries(wavedcc dccengine dccpacket DatagramSocket Threads::Threads ${pigpio_LIBRARY})
target_include_directories(wavedccd PRIVATE ${pigpio_INCLUDE_DIRS} )
target_link_libraries(wavedccd dccengine dccpacket DatagramSocket Threads::Threads ${pigpio_LIBRARY})

else()  #default is to use the pigpiod interface... (USE_PIGPIOD_IF still works)

set(CMAKE_CXX_FLAGS "-DUSE_PIGPIOD_IF")
target_include_directories(wavedcc SYSTEM PUBLIC ${PIGPIO_INCLUDE_DIR})
target_link_libraries(wavedcc dccengine dccpacket DatagramSocket Threads::Threads ${pigpiod_if2_LIBRARY})
target_include_directories(wavedccd SYSTEM PUBLIC ${PIGPIO_INCLUDE_DIR})
target_link_libraries(wavedccd dccengine dccpacket DatagramSocket Threads::Threads ${pigpiod_if2_LIBRARY})

endif()
