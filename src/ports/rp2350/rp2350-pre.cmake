set(PICO_PLATFORM rp2350-arm-s)

IF ( NOT DEFINED ENV{PICO_SDK_PATH})
    set(ENV{PICO_SDK_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/libs/pico-sdk)
ENDIF()
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
