OSM_DIR ?= .
BUILD_DIR ?= build

GIT_COMMITS := $(shell git rev-list --count HEAD)
GIT_COMMIT := $(shell git log -n 1 --format="%h-%f")
GIT_SHA1 := $(shell git log -n 1 --format="%h")
GIT_TAG ?= $(shell git tag --points-at HEAD)

MODELS = $(shell find model/* -type d -printf '%f\n')
MODELS_FW = $(MODELS:%=$(BUILD_DIR)/%/complete.bin)

RELEASE_DIR := releases
JSON_CONV_DIR := $(OSM_DIR)/tools/img_json_interpretter


RELEASE_NAME := $(GIT_TAG)_release_bundle

JSON_CONV := $(JSON_CONV_DIR)/build/json_x_img

all: $(MODELS_FW)

$(JSON_CONV) : $(LIBOPENCM3)
	$(MAKE) -C $(JSON_CONV_DIR)

$(BUILD_DIR)/.git.$(GIT_COMMIT): $(LIBOPENCM3)
	mkdir -p "$(@D)"
	rm -f $(BUILD_DIR)/.git.*
	touch $@

include stm/stm.mk

define PROGRAM_template
  include $(1)
 endef

PROGRAMS_MKS = $(shell find model -maxdepth 1 -name "*.mk" -printf "%p\n")

# To see what it's doing, just replace "eval" with "info"
$(foreach file_mk,$(PROGRAMS_MKS),$(eval $(call PROGRAM_template,$(file_mk),$(file_mk:model/%.mk=%))))

clean:
	make -C libs/libopencm3 $@ TARGETS=stm32/l4
	make -C $(JSON_CONV_DIR) $@
	rm -rf $(BUILD_DIR)

-include $(shell find "$(BUILD_DIR)" -name "*.d")

