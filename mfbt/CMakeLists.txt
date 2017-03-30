cmake_minimum_required(VERSION 2.8.12)
set(SOURCES
    Assertions.cpp
    ChaosMode.cpp
    double-conversion/bignum-dtoa.cc
    double-conversion/bignum.cc
    double-conversion/cached-powers.cc
    double-conversion/diy-fp.cc
    double-conversion/double-conversion.cc
    double-conversion/fast-dtoa.cc
    double-conversion/fixed-dtoa.cc
    double-conversion/strtod.cc
    FloatingPoint.cpp
    HashFunctions.cpp
    JSONWriter.cpp
    Poison.cpp
    SHA1.cpp
    TaggedAnonymousMemory.cpp
    Unused.cpp
    Compression.cpp
    decimal/Decimal.cpp
    )

add_library(mfbt STATIC ${SOURCES})
target_include_directories(mfbt PUBLIC . )
