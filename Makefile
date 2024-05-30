OSM_DIR ?= .
BUILD_DIR ?= $(OSM_DIR)/build
MODEL_DIR ?= $(OSM_DIR)/model
LIB_DIR ?= $(OSM_DIR)/libs

GIT_COMMITS := $(shell git rev-list --count HEAD)
GIT_COMMIT := $(shell git log -n 1 --format="%h-%f")
GIT_SHA1 := $(shell printf "%.*s\n" 7 $$(git log -n 1 --format="%H"))
GIT_SHA1_LEN := $(shell printf '%s' '$(GIT_SHA1)' | wc -c)
GIT_TAG := $(shell git describe --tags --exact-match --abbrev=0 2> /dev/null)
GIT_VERSION ?= $(shell if [ -z "$(GIT_TAG)" ]; then \
    echo "[$(GIT_COMMITS)]-$(GIT_COMMIT)"; \
  else \
    echo "[$(GIT_COMMITS)]-$(GIT_SHA1)-$(GIT_TAG)"; \
  fi \
  )


MODELS = $(shell find $(MODEL_DIR)/* -maxdepth 0 -type d -printf '%f\n')
MODELS_FW = $(MODELS:%=$(BUILD_DIR)/%/.complete)
MODELS_CPPCHECK = $(MODELS:%=%_cppcheck)

RELEASE_DIR ?= $(BUILD_DIR)/releases
RELEASE_NAME := $(GIT_TAG)_release_bundle

REAL_MODELS = $(shell find $(MODEL_DIR)/* -maxdepth 0 -type d ! -name "*penguin*" -printf '%f\n')
MODELS_RELEASE_BUNDLES =$(REAL_MODELS:%=$(RELEASE_DIR)/%_$(RELEASE_NAME).tar.gz)


default: all

release : $(MODELS_RELEASE_BUNDLES)

all: $(MODELS_FW)
	@if [ $(GIT_SHA1_LEN) != 7 ]; then \
		echo "Error: git sha is incorrect length."; \
	fi

$(BUILD_DIR)/.git.$(GIT_COMMIT): $(LIBOPENCM3)
	mkdir -p "$(@D)"
	rm -f $(BUILD_DIR)/.git.*
	touch $@


include $(OSM_DIR)/ports/base.mk
include $(OSM_DIR)/ports/stm/stm.mk
include $(OSM_DIR)/ports/linux/linux.mk
include $(OSM_DIR)/ports/esp32/esp32.mk

define PROGRAM_template
  include $(1)
endef

PROGRAMS_MKS = $(shell find $(MODEL_DIR) -maxdepth 2 -name "*.mk" -printf "%p\n")

# To see what it's doing, just replace "eval" with "info"
$(foreach file_mk,$(PROGRAMS_MKS),$(eval $(call PROGRAM_template,$(file_mk))))

cppcheck: $(MODELS_CPPCHECK)


clean:
	make -C $(OSM_DIR)/libs/libopencm3 $@ TARGETS=stm32/l4
	rm -rf $(BUILD_DIR)

-include $(shell find "$(BUILD_DIR)" -name "*.d")

include $(OSM_DIR)/tests/tests.mk
