cmake_minimum_required(VERSION 3.14)
project(dynRtspOpenRtsp)

set(CMAKE_CXX_STANDARD 14)

add_executable(dynRtspProxyServer
        openRTSP.cpp
        playCommon.cpp
        playCommon.hh
        arguments.cpp
        usage.cpp
        separteExterns.h
        shutdown.cpp
        )

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt5 COMPONENTS Core Network REQUIRED)

find_path(BasicUsageEnvironment_INCLUDE_DIR BasicUsageEnvironment.hh PATHS dev/live555/include/BasicUsageEnvironment)
message(STATUS "BasicUsageEnvironment_INCLUDE_DIR=${BasicUsageEnvironment_INCLUDE_DIR}")

find_path(UsageEnvironment_INCLUDE_DIR UsageEnvironment.hh PATHS dev/live555/include/UsageEnvironment)
message(STATUS "UsageEnvironment_INCLUDE_DIR=${UsageEnvironment_INCLUDE_DIR}")

find_path(liveMedia_INCLUDE_DIR RTSPServer.hh PATHS dev/live555/include/liveMedia)
message(STATUS "liveMedia_INCLUDE_DIR=${liveMedia_INCLUDE_DIR}")

find_path(groupsock_INCLUDE_DIR Groupsock.hh PATHS dev/live555/include/groupsock)
message(STATUS "groupsock_INCLUDE_DIR=${groupsock_INCLUDE_DIR}")

find_library(BasicUsageEnvironment_LIB BasicUsageEnvironment PATHS dev/live555/lib)
message(STATUS "BasicUsageEnvironment_LIB=${BasicUsageEnvironment_LIB}")

find_library(UsageEnvironment_LIB UsageEnvironment PATHS dev/live555/lib)
message(STATUS "UsageEnvironment_LIB=${UsageEnvironment_LIB}")

find_library(liveMedia_LIB liveMedia PATHS dev/live555/lib)
message(STATUS "liveMedia_LIB=${liveMedia_LIB}")

find_library(groupsock_LIB groupsock PATHS dev/live555/lib)
message(STATUS "groupsock_LIB=${groupsock_LIB}")

add_library(usageEnvironment STATIC IMPORTED )
set_target_properties(usageEnvironment PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${UsageEnvironment_INCLUDE_DIR}
        IMPORTED_LOCATION ${UsageEnvironment_LIB})
add_library(basicUsageEnvironment STATIC IMPORTED )
set_target_properties(basicUsageEnvironment PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${BasicUsageEnvironment_INCLUDE_DIR}
        IMPORTED_LOCATION ${BasicUsageEnvironment_LIB})
add_library(liveMedia STATIC IMPORTED )
set_target_properties(liveMedia PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${liveMedia_INCLUDE_DIR}
        IMPORTED_LOCATION ${liveMedia_LIB})
add_library(groupsock STATIC IMPORTED )
set_target_properties(groupsock PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${groupsock_INCLUDE_DIR}
        IMPORTED_LOCATION ${groupsock_LIB})

#target_include_directories(dynRtspProxyServer PRIVATE
#        ${live555_INCLUDE_DIR})

if (MSVC)
    message(STATIC "MSVC")
    target_compile_definitions(dynRtspProxyServer
            PRIVATE _LARGEFILE_SOURCE=1 _FILE_OFFSET_BITS=64 BSD=1 _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0A00)
    target_link_libraries(dynRtspProxyServer
            PRIVATE
                liveMedia
                basicUsageEnvironment
                usageEnvironment
                groupsock
                ws2_32
                winmm
            )
else()
    target_compile_definitions(dynRtspProxyServer
            PRIVATE SOCKLEN_T=socklen_t _LARGEFILE_SOURCE=1 _FILE_OFFSET_BITS=64 BSD=1)
    target_link_libraries(dynRtspProxyServer PRIVATE
            liveMedia
            basicUsageEnvironment
            usageEnvironment
            groupsock
            pthread)
endif()
target_compile_options(dynRtspProxyServer PRIVATE -Wall)