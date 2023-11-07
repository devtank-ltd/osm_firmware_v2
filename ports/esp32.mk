define ESP32_FIRMWARE
$(call PORT_BASE_RULES,$(1),ESP32)

$(1): $$(BUILD_DIR)/$(1)/.complete

ifneq ($$(origin IDF_PATH), undefined)
$$(BUILD_DIR)/$(1)/build.ninja:
	mkdir -p $$(BUILD_DIR)/$(1)
	IDF_TARGET=esp32 GIT_COMMITS="$(GIT_COMMITS)" GIT_COMMIT="$(GIT_COMMIT)" GIT_SHA1="$(GIT_SHA1)" cmake $$(MODEL_DIR)/$(1)/ -B $$(BUILD_DIR)/$(1)/ -G Ninja


$$(BUILD_DIR)/$(1)/$(1).bin: $$(BUILD_DIR)/$(1)/build.ninja
	ninja -C $$(BUILD_DIR)/$(1)

$(1)_flash:
	IDF_TARGET=esp32 GIT_COMMITS="$(GIT_COMMITS)" GIT_COMMIT="$(GIT_COMMIT)" GIT_SHA1="$(GIT_SHA1)" ninja -C $$(BUILD_DIR)/$(1) -f build.ninja flash

$(1)_monitor:
	ninja -C $$(BUILD_DIR)/$(1) -f build.ninja monitor

$(1)_build:
	IDF_TARGET=esp32 GIT_COMMITS="$(GIT_COMMITS)" GIT_COMMIT="$(GIT_COMMIT)" GIT_SHA1="$(GIT_SHA1)" ninja -C $$(BUILD_DIR)/$(1) -f build.ninja

$$(BUILD_DIR)/$(1)/.complete: $$(BUILD_DIR)/$(1)/$(1).bin

else
$$(info $(1) requires ESP IDF setup)
$$(BUILD_DIR)/$(1)/.complete:
	mkdir -p $$(BUILD_DIR)/$(1)
	touch $$@
endif
endef
