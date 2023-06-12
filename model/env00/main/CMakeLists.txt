idf_component_register(SRC_DIRS "../../../ports/esp32/src" "../../../core/src"
                    INCLUDE_DIRS "./" "../../../ports/esp32/include" "../../../core/include" "../../../sensors/include" "../../../comms/include" "../../../protocols/include")
add_definitions(-Dfw_name=env00 -DFW_NAME=ENV00 -DGIT_VERSION="$ENV{GIT_COMMITS}-$ENV{GIT_COMMIT}" -DGIT_SHA1="$ENV{GIT_SHA1}")
