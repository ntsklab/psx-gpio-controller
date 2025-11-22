cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(ENV{PICO_SDK_PATH} ${CMAKE_CURRENT_LIST_DIR}/../pico-sdk)
set(PICOTOOL_FETCH_FROM_GIT_PATH ${CMAKE_CURRENT_LIST_DIR}/../picotool)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

set(target_name PicoGamepadConverter)

project(${target_name} C CXX ASM)

pico_sdk_init()

add_executable(${target_name})

target_sources(${target_name} PRIVATE
 psx_gpio_simulator.c
 utils.c
 )

add_subdirectory(device_files/psx-device)
add_subdirectory(flash-lib)

pico_enable_stdio_usb(${target_name} 1)
pico_enable_stdio_uart(${target_name} 0)

 # print memory usage, enable all warnings
target_link_options(${target_name} PRIVATE -Xlinker --print-memory-usage)
target_compile_options(${target_name} PRIVATE -Wall -Wextra)

# needed so we can find tusb_config.h (kept for compatibility)
target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_LIST_DIR} device_files/psx-device ${CMAKE_BINARY_DIR}/device_files/psx-device)

# Link libraries - stdio over USB via pico_stdio_usb (built-in to pico_stdlib)
target_link_libraries(${target_name} PRIVATE pico_stdlib pico_multicore pico_status_led flash-lib psx-device)

pico_add_extra_outputs(${target_name})

