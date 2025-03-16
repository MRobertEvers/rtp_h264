cmake_minimum_required(VERSION 3.7)
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()


project(rtpencoder
	LANGUAGES C
)
set(PROJECT_DESCRIPTION "Sample RTP Streamer")

cmake_policy(SET CMP0065 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

# Default is a debug build
set(CMAKE_C_FLAGS_MODE "-O0 -g")
set(CMAKE_CXX_FLAGS_MODE " -O0 -g")
set(CMAKE_EXE_LINKER_FLAGS_MODE  "")

message(STATUS "Building for ${CMAKE_BUILD_TYPE}")
if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
	set(CMAKE_C_FLAGS_MODE "-Os -s")
	set(CMAKE_CXX_FLAGS_MODE "-Os -s")
	set(CMAKE_EXE_LINKER_FLAGS_MODE  "-Wl,-rpath /")
endif()


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_MODE} -ffp-contract=off")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_MODE} -ffp-contract=off")
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS_MODE}")

if(POLICY CMP0079)
	cmake_policy(SET CMP0079 NEW)
endif()

add_executable(sample
	sample/main.c
	src/rtp_h264.c
)

target_include_directories(sample PRIVATE ./include)

set_target_properties(sample PROPERTIES
	CXX_STANDARD 17
	OUTPUT_NAME sample)


