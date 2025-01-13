cmake_minimum_required(VERSION 3.13)

include($ENV{OSM_DIR}/src/ports/rp2350/rp2350-pre.cmake)

project(osm_firmware C CXX ASM)

include($ENV{OSM_DIR}/src/ports/rp2350/rp2350-post.cmake)

add_definitions(-Dfw_name=pp01a -DFW_NAME=PP01A)

add_executable(firmware
    $ENV{OSM_MODEL_DIR}/pp01a/main/model.c
    $ENV{OSM_DIR}/ports/rp2350/src/rp2350.c
    $ENV{OSM_DIR}/ports/rp2350/src/persist_config.c
    $ENV{OSM_DIR}/ports/rp2350/src/sleep.c
    $ENV{OSM_DIR}/ports/rp2350/src/uarts.c
    $ENV{OSM_DIR}/ports/rp2350/src/rp2350_comms.c
    $ENV{OSM_DIR}/core/src/main.c
    $ENV{OSM_DIR}/core/src/base.c
    $ENV{OSM_DIR}/core/src/cmd.c
    $ENV{OSM_DIR}/core/src/common.c
    $ENV{OSM_DIR}/core/src/log.c
    $ENV{OSM_DIR}/core/src/measurements.c
    $ENV{OSM_DIR}/core/src/measurements_mem.c
    $ENV{OSM_DIR}/core/src/persist_base.c
    $ENV{OSM_DIR}/core/src/platform_common.c
    $ENV{OSM_DIR}/core/src/ring.c
    $ENV{OSM_DIR}/core/src/uart_rings.c
    $ENV{OSM_DIR}/protocols/src/jsonblob.c
    $ENV{OSM_DIR}/protocols/src/comms_behind.c
    $ENV{OSM_DIR}/sensors/src/fw.c
)

target_link_libraries(firmware
    pico_stdlib
    pico_unique_id
    pico_time
    hardware_clocks
    hardware_gpio
    hardware_watchdog
    hardware_irq
    hardware_dma
)

pico_add_extra_outputs(firmware)

target_include_directories(firmware PUBLIC
    $ENV{OSM_MODEL_DIR}/pp01a/main
    $ENV{OSM_DIR}/ports/rp2350/include
    $ENV{OSM_DIR}/core/include
    $ENV{OSM_DIR}/sensors/include
    $ENV{OSM_DIR}/comms/include
    $ENV{OSM_DIR}/protocols/include
)

set_target_properties(firmware PROPERTIES PICO_TARGET_LINKER_SCRIPT
    $ENV{OSM_DIR}/ports/rp2350/rp2350.ld)
