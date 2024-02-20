idf_component_register(SRC_DIRS "." "../../../ports/esp32/src" "../../../core/src" "../../../core/src" "../../../comms/src"
                    INCLUDE_DIRS "./" "../../../ports/esp32/include" "../../../core/include" "../../../sensors/include" "../../../comms/include" "../../../protocols/include")
add_definitions(-Dfw_name=env01a -DFW_NAME=ENV01A -DGIT_VERSION="\""$ENV{GIT_COMMITS}-$ENV{GIT_COMMIT}"\"" -DGIT_SHA1="$ENV{GIT_SHA1}")
