define RP2350_FIRMWARE
$(call PORT_BASE_RULES,$(1),RP2350)

$(1): $$(OSM_BUILD_DIR)/$(1)/.complete

ifneq ($$(origin PICO_SDK_PATH), undefined)
$$(OSM_BUILD_DIR)/$(1)/Makefile:
	mkdir -p $$(OSM_BUILD_DIR)/$(1)
	OSM_DIR="$$(shell realpath $$(OSM_DIR))" OSM_MODEL_DIR="$$(shell realpath $$(OSM_MODEL_DIR))" OSM_GIT_VERSION="$$(OSM_GIT_VERSION)" OSM_GIT_SHA1="$$(OSM_GIT_SHA1)" OSM_SRCS="$$($(1)_SOURCES)" cmake $$(OSM_MODEL_DIR)/$(1)/ -B $$(OSM_BUILD_DIR)/$(1)/


$$(OSM_BUILD_DIR)/$(1)/$(1).bin: $$(OSM_BUILD_DIR)/$(1)/Makefile
	OSM_DIR="$$(shell realpath $(OSM_DIR))" OSM_MODEL_DIR="$(shell realpath $(OSM_MODEL_DIR))" OSM_GIT_VERSION="$(OSM_GIT_VERSION)" OSM_GIT_SHA1="$(OSM_GIT_SHA1)" OSM_SRCS="$$($(1)_SOURCES)" make -C $$(OSM_BUILD_DIR)/$(1)

$$(OSM_BUILD_DIR)/$(1)/.complete: $$(OSM_BUILD_DIR)/$(1)/$(1).bin
	mkdir -p $$(OSM_BUILD_DIR)/$(1)
	touch $$@

else
$$(info $(1) requires PICO SDK setup)
$$(OSM_BUILD_DIR)/$(1)/.complete:
	mkdir -p $$(OSM_BUILD_DIR)/$(1)
	touch $$@
endif

$(1)_flash: $$(OSM_BUILD_DIR)/$(1)/bootloader.bin $$(OSM_BUILD_DIR)/$(1)/application.bin
	openocd -f interface/cmsis-dap.cfg \
	        -f target/rp2350.cfg \
	        -c "init" -c "reset init" \
	        -c "program $$(OSM_BUILD_DIR)/$(1)/bootloader.bin 0x10000000 verify reset" \
	        -c "program $$(OSM_BUILD_DIR)/$(1)/application.bin 0x1000a000 verify reset exit" \
	        -c "reset" \
	        -c "shutdown"
endef
