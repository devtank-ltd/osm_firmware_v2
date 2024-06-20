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
REAL_MODELS_FW = $(REAL_MODELS:%=$(BUILD_DIR)/%/complete.bin)
FW_VERSION_DIR:= $(BUILD_DIR)/fw_releases
FW_VERSION_TAR:= $(BUILD_DIR)/fw_releases.tar.xz
FW_VERSION_INFO := $(FW_VERSION_DIR)/latest_fw_info.json

default: all

release : $(MODELS_RELEASE_BUNDLES)

$(FW_VERSION_TAR): $(REAL_MODELS_FW)
	if [ -n "$(GIT_TAG)" ]; \
	then \
	  mkdir -p $(FW_VERSION_DIR);\
	  echo "[" > $(FW_VERSION_INFO);\
	  for n in $(REAL_MODELS); \
	  do \
	    fw=$${n}_$(GIT_TAG).bin; \
	    cp -v $(BUILD_DIR)/$$n/complete.bin $(FW_VERSION_DIR)/$$fw; \
	    echo "  {\"tag\": \"$(GIT_TAG)\", \"sha\": \"$(GIT_SHA1)\", \"path\": \"$${fw}\"}," >> $(FW_VERSION_INFO); \
	  done; \
	  echo "]" >> $(FW_VERSION_INFO); \
	  touch $@; \
	  tar Jcf $@ $(FW_VERSION_DIR); \
	else \
	  echo "No git tag for release.";\
	fi

fw_releases: $(FW_VERSION_TAR)

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
