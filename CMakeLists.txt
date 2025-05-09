cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

# Define the project name and type
project(midi_v1 C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# Initialize the SDK
pico_sdk_init()

# Extra compiler settings
add_compile_options(-Os -fwrapv -Wall )

# Say what exe we are trying to build
add_executable(midi_v1
# source files needed are:
        midi_main.c
        byte_queue.c
        )

# Where do we need to look to find stuff?
target_include_directories(midi_v1 PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# pull in common dependencies and additional uart hardware support
target_link_libraries(midi_v1 PRIVATE pico_stdlib pico_multicore hardware_uart hardware_adc pico_unique_id)

# create map/bin/hex file etc.
pico_add_extra_outputs(midi_v1)

# add url via pico_set_program_url
# example_auto_set_url(midi_v1)
