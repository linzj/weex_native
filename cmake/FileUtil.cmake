macro(ADD_TO_INCLUDES srcDir dstDir)
    file(GLOB _files "${srcDir}/*.h")
    file(COPY ${_files}
        DESTINATION ${CMAKE_BINARY_DIR}/includes/${dstDir})
endmacro()

set(ION_INCLUDES
${CMAKE_BINARY_DIR}/includes
)

include_directories(
${ION_INCLUDES}
)
