cmake_minimum_required(VERSION 2.8.12)
project(weexv8)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(Android)
include(FileUtil)
ADD_TO_INCLUDES(mfbt mozilla)
ADD_TO_INCLUDES(mfbt/double-conversion mozilla)
ADD_TO_INCLUDES(mozglue/misc mozilla)
ADD_TO_INCLUDES(js/public js)
ADD_TO_INCLUDES(configured_header .)

set(ION_DEFINITIONS "")
string(REPLACE " " ";" DEFINTIONS_LIST $ENV{ION_DEFINITION})
foreach(definition ${DEFINTIONS_LIST})
    list(APPEND ION_DEFINITIONS "-D${definition}")
endforeach()

add_definitions(
${ION_DEFINITIONS}
)

add_subdirectory(mfbt)
add_subdirectory(fdlibm)
add_subdirectory(mozglue)
add_subdirectory(js)
