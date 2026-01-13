cmake_minimum_required(VERSION 3.25)

# Set pico-sdk path (using the one from your OpenTrickler project)
set(PICO_SDK_PATH "${CMAKE_SOURCE_DIR}/../library/pico-sdk")

# Set board to Pico 2W
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Include the SDK
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(wifi_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Pull in FreeRTOS
set(FREERTOS_KERNEL_PATH "${CMAKE_SOURCE_DIR}/../library/FreeRTOS-Kernel")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# Build the access point example (FreeRTOS version)
add_executable(picow_access_point
    picow_access_point.c
    dhcpserver/dhcpserver.c
    dnsserver/dnsserver.c
)

target_include_directories(picow_access_point PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/dhcpserver
    ${CMAKE_CURRENT_LIST_DIR}/dnsserver
)

target_link_libraries(picow_access_point
    pico_cyw43_arch_lwip_sys_freertos
    pico_stdlib
    hardware_flash
    hardware_sync
    hardware_watchdog
    FreeRTOS-Kernel-Heap4
)

# Configure IP address for access point (192.168.4.1) and FreeRTOS
target_compile_definitions(picow_access_point PRIVATE
    CYW43_DEFAULT_IP_AP_ADDRESS=0xC0A80401
    NO_SYS=0
)

pico_add_extra_outputs(picow_access_point)

# Enable USB output for debugging
pico_enable_stdio_usb(picow_access_point 1)
pico_enable_stdio_uart(picow_access_point 1)
