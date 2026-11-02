cmake_minimum_required(VERSION 3.13)

include($ENV{OSM_DIR}/src/ports/rp2350/rp2350-pre.cmake)

project(osm_firmware C CXX ASM)

# TODO: these all call be refactored as we know the firmware name already.
# Does this file even need to exist?
add_definitions(-Dfw_name=loti2 -DFW_NAME=LOTI2)

include($ENV{OSM_DIR}/src/ports/rp2350/rp2350-post.cmake)
