define ESP32_FIRMWARE
$(call PORT_BASE_RULES,$(1),ESP32)

$(1): $$(OSM_BUILD_DIR)/$(1)/.complete

ifneq ($$(origin IDF_PATH), undefined)
$$(OSM_BUILD_DIR)/$(1)/build.ninja:
	mkdir -p $$(OSM_BUILD_DIR)/$(1)
	IDF_TARGET=esp32 OSM_GIT_VERSION="$(OSM_GIT_VERSION)" OSM_GIT_SHA1="$(OSM_GIT_SHA1)" cmake $$(OSM_MODEL_DIR)/$(1)/ -B $$(OSM_BUILD_DIR)/$(1)/ -G Ninja


$$(OSM_BUILD_DIR)/$(1)/$(1).bin: $$(OSM_BUILD_DIR)/$(1)/build.ninja
	ninja -C $$(OSM_BUILD_DIR)/$(1)

$(1)_flash:
	IDF_TARGET=esp32 OSM_GIT_VERSION="$(OSM_GIT_VERSION)" OSM_GIT_SHA1="$(OSM_GIT_SHA1)" ninja -C $$(OSM_BUILD_DIR)/$(1) -f build.ninja flash

$(1)_monitor:
	ninja -C $$(OSM_BUILD_DIR)/$(1) -f build.ninja monitor

$(1)_build:
	IDF_TARGET=esp32 OSM_GIT_VERSION="$(OSM_GIT_VERSION)" OSM_GIT_SHA1="$(OSM_GIT_SHA1)" ninja -C $$(OSM_BUILD_DIR)/$(1) -f build.ninja

$$(OSM_BUILD_DIR)/$(1)/.complete: $$(OSM_BUILD_DIR)/$(1)/$(1).bin

else
$$(info $(1) requires ESP IDF setup)
$$(OSM_BUILD_DIR)/$(1)/.complete:
	mkdir -p $$(OSM_BUILD_DIR)/$(1)
	touch $$@
endif
endef
