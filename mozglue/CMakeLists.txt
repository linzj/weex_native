cmake_minimum_required(VERSION 2.8.12)

set(MISC_SOURCES
    misc/StackWalk.cpp
    misc/TimeStamp.cpp
    misc/TimeStamp_posix.cpp
)

add_library(misc
STATIC
${MISC_SOURCES})
