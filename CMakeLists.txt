cmake_minimum_required(VERSION 3.12)

set(PICO_SDK_PATH ../pico-sdk)

# Pull in SDK (must be before project)
include(pico-sdk/external/pico_sdk_import.cmake)

project(jacscript_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

if(PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# set(PICO_EXAMPLES_PATH ${PROJECT_SOURCE_DIR})

add_compile_options(-Wall
    -Wno-format
    -Wno-unused-function
    -Wno-maybe-uninitialized
    -Wno-zero-length-bounds
    -ffunction-sections
)

include(build/config.cmake)

pico_sdk_init()

add_subdirectory(src)

# location of jd_user_config.h
set(JACDAC_USER_CONFIG_DIR "../src")
add_subdirectory(jacdac-c)
