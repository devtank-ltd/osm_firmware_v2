define ESP32_FIRMWARE
$(call PORT_BASE_RULES,$(1),ESP32)


$$(BUILD_DIR)/$(1)/build.ninja:
	mkdir -p $$(BUILD_DIR)/$(1)
	IDF_TARGET=esp32 cmake $$(MODEL_DIR)/$(1)/ -B $$(BUILD_DIR)/$(1)/ -G Ninja


$$(BUILD_DIR)/$(1)/$(1).bin: $$(BUILD_DIR)/$(1)/build.ninja
	ninja -C $$(BUILD_DIR)/$(1)


$$(BUILD_DIR)/$(1)/.complete: $$(BUILD_DIR)/$(1)/$(1).bin

$(1): $$(BUILD_DIR)/$(1)/.complete

endef
