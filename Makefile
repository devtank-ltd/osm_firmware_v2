OSM_DIR ?= .
BUILD_DIR ?= $(OSM_DIR)/build
MODEL_DIR ?= $(OSM_DIR)/model

GIT_COMMITS := $(shell git rev-list --count HEAD)
GIT_COMMIT := $(shell git log -n 1 --format="%h-%f")
GIT_SHA1 := $(shell git log -n 1 --format="%h")
GIT_TAG ?= $(shell git tag --points-at HEAD)

MODELS = $(shell find $(MODEL_DIR)/* -maxdepth 0 -type d -printf '%f\n')
MODELS_FW = $(MODELS:%=$(BUILD_DIR)/%/.complete)
MODELS_CPPCHECK = $(MODELS:%=%_cppcheck)

RELEASE_DIR ?= $(BUILD_DIR)/releases


RELEASE_NAME := $(GIT_TAG)_release_bundle

all: $(MODELS_FW)

$(BUILD_DIR)/.git.$(GIT_COMMIT): $(LIBOPENCM3)
	mkdir -p "$(@D)"
	rm -f $(BUILD_DIR)/.git.*
	touch $@


include $(OSM_DIR)/ports/base.mk
include $(OSM_DIR)/ports/stm.mk
include $(OSM_DIR)/ports/linux.mk
include $(OSM_DIR)/ports/esp32.mk

define PROGRAM_template
  include $(1)
endef

PROGRAMS_MKS = $(shell find $(MODEL_DIR) -maxdepth 1 -name "*.mk" -printf "%p\n")

# To see what it's doing, just replace "eval" with "info"
$(foreach file_mk,$(PROGRAMS_MKS),$(eval $(call PROGRAM_template,$(file_mk))))

cppcheck: $(MODELS_CPPCHECK)


clean:
	make -C $(OSM_DIR)/libs/libopencm3 $@ TARGETS=stm32/l4
	rm -rf $(BUILD_DIR)

-include $(shell find "$(BUILD_DIR)" -name "*.d")
