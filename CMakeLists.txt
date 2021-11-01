cmake_minimum_required(VERSION 3.21)
project(pdec)

file(GLOB_RECURSE PngDeCoder "${PROJECT_SOURCE_DIR}/src/PngDeCoder/pdec.h"
"${PROJECT_SOURCE_DIR}/src/PngDeCoder/pdec.c")

add_executable(${PROJECT_NAME} ${PngDeCoder} "${PROJECT_SOURCE_DIR}/src/test.c")

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    add_definitions(/MP)
    add_definitions(/W4)
    add_definitions(/WX)
endif()

# options
macro(set_option option help value)
    option(${option} ${help} ${value})
    if(${option})
        message(STATUS "${PROJECT_NAME}: ${option}: ON")
        target_compile_definitions(${PROJECT_NAME} PUBLIC ${option})
    else()
        message(STATUS "${PROJECT_NAME}: ${option}: OFF")
    endif()
endmacro()

set_option(PDEC_REVERSE_ENDIANNESS "Reverse endianness when doing pointer arithmetic (keep ON if you're loading bitmap into memory)" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)