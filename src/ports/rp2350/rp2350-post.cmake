set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_definitions(-DGIT_VERSION="\""$ENV{OSM_GIT_VERSION}"\"" -DGIT_SHA1="$ENV{OSM_GIT_SHA1}")

add_compile_options(-Wall
    -Werror
    -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
    -Wno-unused-function # we have some for the docs that aren't called
    -Wno-unused-parameter
    -Wno-maybe-uninitialized
    -Wno-discarded-qualifier
    -Wextra
    -Wformat=2
    -Wcast-align
    -Wpointer-arith
    -Wwrite-strings
    -Wunreachable-code
    -Wstrict-aliasing=2
    -ffloat-store
    -fno-common
    -fstrict-aliasing
)

separate_arguments(OSM_SRCS UNIX_COMMAND PROGRAM SEPARATE_ARGS $ENV{OSM_SRCS})

list(TRANSFORM OSM_SRCS PREPEND $ENV{OSM_DIR}/)

add_executable(application)

pico_generate_pio_header(application
    ${CMAKE_CURRENT_LIST_DIR}/i2s.pio
    ${CMAKE_CURRENT_LIST_DIR}/uarts.pio
)

target_sources(application PRIVATE ${OSM_SRCS})

target_link_libraries(application
    pico_stdlib
    pico_unique_id
    pico_time
    hardware_clocks
    hardware_gpio
    hardware_watchdog
    hardware_irq
    hardware_dma
    hardware_flash
    hardware_pio
)

pico_add_extra_outputs(application)

target_include_directories(application PUBLIC
    $ENV{OSM_MODEL_DIR}/pp01a/main
    $ENV{OSM_DIR}/include/osm/ports/rp2350
    $ENV{OSM_DIR}/include
)

set_target_properties(application
    PROPERTIES PICO_TARGET_LINKER_SCRIPT $ENV{OSM_DIR}/src/ports/rp2350/rp2350.ld
)

add_executable(bootloader
    $ENV{OSM_DIR}/src/ports/rp2350/bootloader/bootloader.c
)

target_link_libraries(bootloader
    pico_stdlib
    hardware_gpio
    hardware_uart
    hardware_watchdog
    hardware_flash
    hardware_divider
)

pico_add_extra_outputs(bootloader)

target_include_directories(bootloader PUBLIC
    $ENV{OSM_DIR}/src/ports/rp2350/bootloader
    $ENV{OSM_MODEL_DIR}/pp01a/main
    $ENV{OSM_DIR}/include/osm/ports/rp2350
    $ENV{OSM_DIR}/include
)

set_target_properties(bootloader
    PROPERTIES PICO_TARGET_LINKER_SCRIPT $ENV{OSM_DIR}/src/ports/rp2350/bootloader/rp2350_bootloader.ld
)

add_custom_target(firmware ALL
    COMMAND
        ${BASH}
        dd if=bootloader.bin of=firmware.bin bs=256 &&
        dd if=application.bin of=firmware.bin bs=256 seek=160 &&
        chmod +x firmware.bin &&
        ${picotool_INSTALL_DIR}/picotool/picotool uf2 convert firmware.bin firmware.uf2 --family rp2350-arm-s --abs-block
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Combining bootloader and application"
)

add_dependencies(firmware
    bootloader
    application
)
