idf_component_register(SRC_DIRS "." "../../../src/ports/esp32" "../../../src/core"
                    INCLUDE_DIRS "./" "../../../include" "../../../include/osm/ports/esp32")
	    add_definitions(-Dfw_name=env01a -DFW_NAME=ENV01A -Dcomms_name=esp_comms -DGIT_VERSION="\""$ENV{OSM_GIT_VERSION}"\"" -DGIT_SHA1="$ENV{OSM_GIT_SHA1}")
