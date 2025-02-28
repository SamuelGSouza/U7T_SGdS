# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(agendamento-eventos C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(agendamento-eventos 
    agendamento-eventos.c 
    inc/ssd1306_i2c.c
)

pico_set_program_name(agendamento-eventos "agendamento-eventos")
pico_set_program_version(agendamento-eventos "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(agendamento-eventos 0)
pico_enable_stdio_usb(agendamento-eventos 1)

# Add the standard library to the build
target_link_libraries(agendamento-eventos
        pico_stdlib
        hardware_i2c
        hardware_gpio
        hardware_adc
        pico_cyw43_arch_lwip_poll
)

# Add the standard include files to the build
target_include_directories(agendamento-eventos PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/inc
)

# Gera o código do arquivo PIO
pico_generate_pio_header(agendamento-eventos ${CMAKE_CURRENT_LIST_DIR}/ws2818b.pio)

pico_add_extra_outputs(agendamento-eventos)
