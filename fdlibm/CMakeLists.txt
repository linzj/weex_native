cmake_minimum_required(VERSION 2.8.12)

file(GLOB SOURCES src/*.cpp)

MESSAGE("SOURCES: ${SOURCES}")

add_library(fdlibm
STATIC
${SOURCES})

target_include_directories(fdlibm
PUBLIC
src)
