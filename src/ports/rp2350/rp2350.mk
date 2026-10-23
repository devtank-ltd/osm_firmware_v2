
$(OSM_BUILD_DIR)/picotool/picotool:
	mkdir -p $(OSM_BUILD_DIR)/picotool
	PICO_SDK_PATH=$(shell realpath $(OSM_DIR)/libs/pico-sdk) cmake $(OSM_DIR)/libs/picotool -B $(OSM_BUILD_DIR)/picotool
	PICO_SDK_PATH=$(shell realpath $(OSM_DIR)/libs/pico-sdk) make -C $(OSM_BUILD_DIR)/picotool


define RP2350_FIRMWARE
$(call PORT_BASE_RULES,$(1),RP2350)

$(1): $$(OSM_BUILD_DIR)/$(1)/.complete

$$(OSM_BUILD_DIR)/$(1)/Makefile: $$($(1)_SOURCES) $$(shell find $$(OSM_DIR) -name "*.h") $$(shell find $$(OSM_MODEL_DIR) -name "*.h")
	mkdir -p $$(OSM_BUILD_DIR)/$(1)
	OSM_DIR="$$(shell realpath $$(OSM_DIR))" \
	OSM_MODEL_NAME=$(1) \
	OSM_MODEL_DIR="$$(shell realpath $$(OSM_MODEL_DIR))" \
	OSM_GIT_VERSION="$$(OSM_GIT_VERSION)" \
	OSM_GIT_SHA1="$$(OSM_GIT_SHA1)" \
	OSM_SRCS="$$($(1)_SOURCES)" \
	PICO_SDK_PATH=$$(shell realpath $$(OSM_DIR)/libs/pico-sdk) cmake $$(OSM_MODEL_DIR)/$(1)/ -B $$(OSM_BUILD_DIR)/$(1)/

$$(OSM_BUILD_DIR)/$(1)/firmware.uf2: $$(OSM_BUILD_DIR)/$(1)/Makefile $$(OSM_BUILD_DIR)/picotool/picotool
	PICO_SDK_PATH=$$(shell realpath $$(OSM_DIR)/libs/pico-sdk) $(MAKE) -C $$(OSM_BUILD_DIR)/$(1)

$$(OSM_BUILD_DIR)/$(1)/.complete: $$(OSM_BUILD_DIR)/$(1)/firmware.uf2
	mkdir -p $$(OSM_BUILD_DIR)/$(1)
	touch $$@

$(1)_flash: $$(OSM_BUILD_DIR)/$(1)/bootloader.bin $$(OSM_BUILD_DIR)/$(1)/application.bin
	openocd -f interface/cmsis-dap.cfg \
	        -f target/rp2350.cfg \
	        -c "init" -c "reset init" \
	        -c "program $$(OSM_BUILD_DIR)/$(1)/bootloader.bin 0x10000000 verify reset" \
	        -c "program $$(OSM_BUILD_DIR)/$(1)/application.bin 0x1000a000 verify reset exit" \
	        -c "reset" \
	        -c "shutdown"

$(1)_picotool: $$(OSM_BUILD_DIR)/$(1)/firmware.uf2
	sudo $$(OSM_BUILD_DIR)/picotool/picotool load $$(OSM_BUILD_DIR)/$(1)/firmware.bin
	sudo $$(OSM_BUILD_DIR)/picotool/picotool reboot

$(1)_picotool_self: $$(OSM_BUILD_DIR)/$(1)/firmware.uf2
	echo "usb_boot" > `ls /dev/serial/by-id/*Devtank_Ltd_$(1)*`
	echo "Waiting for RP2350 Boot device"
	while ! grep "RP2350 Boot" /sys/bus/usb/devices/*/product; do sleep 1; done
	sudo $$(OSM_BUILD_DIR)/picotool/picotool load $$(OSM_BUILD_DIR)/$(1)/firmware.bin
	sudo $$(OSM_BUILD_DIR)/picotool/picotool reboot

endef
