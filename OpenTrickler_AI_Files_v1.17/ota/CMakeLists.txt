# OTA (Over-The-Air) Update Module
#
# Simple OTA without bootloader/partition. Use with caution in production.
#
# To enable: add -DUSE_OTA=ON to cmake command
# Example: cmake -B build -G Ninja -DPICO_BOARD=pico2_w -DUSE_OTA=ON

add_library(ota INTERFACE)

target_sources(ota INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ota.c
    ${CMAKE_CURRENT_LIST_DIR}/rest_ota.c
)

target_include_directories(ota INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(ota INTERFACE
    pico_stdlib
    pico_flash
    pico_sha256
    hardware_flash
    hardware_sync
)

target_compile_definitions(ota INTERFACE
    USE_OTA=1
)
