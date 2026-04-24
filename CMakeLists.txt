cmake_minimum_required(VERSION 3.25)

project(ftracer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FTRACER_SOURCES
        ftracer/main.cpp
        ftracer/FWatcher.cpp)

set(FTRACER_HEADERS
        ftracer/FWatcher.h)

add_executable(FTracer
        ${FTRACER_SOURCES}
        ${FTRACER_HEADERS}
)