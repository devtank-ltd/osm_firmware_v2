cmake_minimum_required(VERSION 3.13)

include($ENV{OSM_DIR}/src/ports/rp2350/rp2350-pre.cmake)

project(osm_firmware C CXX ASM)

add_definitions(-Dfw_name=Loti2Prototype -DFW_NAME=LOTI2PROTOTYPE)

include($ENV{OSM_DIR}/src/ports/rp2350/rp2350-post.cmake)
