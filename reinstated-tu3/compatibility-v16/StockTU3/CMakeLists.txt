# pdzstocktu3 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(pdzstocktu3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(PDZSTOCKTU3_SOURCES
    src/main.cpp
)

if(WIN32)
    add_executable(pdzstocktu3 WIN32 ${PDZSTOCKTU3_SOURCES})
else()
    add_executable(pdzstocktu3 ${PDZSTOCKTU3_SOURCES})
endif()

rexglue_setup_target(pdzstocktu3)
