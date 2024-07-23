OSM_DIR ?= .
OSM_BUILD_DIR ?= $(OSM_DIR)/build
OSM_MODEL_DIR ?= $(OSM_DIR)/model
OSM_LIB_DIR ?= $(OSM_DIR)/libs

OSM_GIT_COMMITS := $(shell git rev-list --count HEAD)
OSM_GIT_COMMIT := $(shell git log -n 1 --format="%h-%f")
OSM_GIT_SHA1 := $(shell printf "%.*s\n" 7 $$(git log -n 1 --format="%H"))
OSM_GIT_SHA1_LEN := $(shell printf '%s' '$(OSM_GIT_SHA1)' | wc -c)
OSM_GIT_TAG := $(shell git describe --tags --exact-match --abbrev=0 2> /dev/null)
OSM_GIT_VERSION ?= $(shell if [ -z "$(OSM_GIT_TAG)" ]; then \
    echo "[$(OSM_GIT_COMMITS)]-$(OSM_GIT_COMMIT)"; \
  else \
    echo "[$(OSM_GIT_COMMITS)]-$(OSM_GIT_SHA1)-$(OSM_GIT_TAG)"; \
  fi \
  )


OSM_MODELS = $(shell find $(OSM_MODEL_DIR)/* -maxdepth 0 -type d -printf '%f\n')
OSM_MODELS_FW = $(OSM_MODELS:%=$(OSM_BUILD_DIR)/%/.complete)
OSM_MODELS_CPPCHECK = $(OSM_MODELS:%=%_cppcheck)

OSM_RELEASE_DIR ?= $(OSM_BUILD_DIR)/releases
OSM_RELEASE_NAME := $(OSM_GIT_TAG)_release_bundle

REAL_OSM_MODELS = $(shell find $(OSM_MODEL_DIR)/* -maxdepth 0 -type d ! -name "*penguin*" -printf '%f\n')
OSM_MODELS_RELEASE_BUNDLES =$(REAL_OSM_MODELS:%=$(OSM_RELEASE_DIR)/%_$(OSM_RELEASE_NAME).tar.gz)
REAL_OSM_MODELS_FW = $(REAL_OSM_MODELS:%=$(OSM_BUILD_DIR)/%/complete.bin)
OSM_FW_VERSION_DIR:= $(OSM_BUILD_DIR)/fw_releases.$(OSM_GIT_TAG)
OSM_FW_VERSION_TAR:= $(OSM_FW_VERSION_DIR).tar.xz
OSM_FW_VERSION_INFO := $(OSM_FW_VERSION_DIR)/latest_fw_info.json

osm_default: osm_all

osm_release : $(OSM_MODELS_RELEASE_BUNDLES)

$(OSM_FW_VERSION_TAR): $(REAL_OSM_MODELS_FW)
	git_tag=$(OSM_GIT_TAG); if [ "$${git_tag#*release}" != "$(OSM_GIT_TAG)"  ]; \
	then \
	  mkdir -p $(OSM_FW_VERSION_DIR);\
	  echo "[" > $(OSM_FW_VERSION_INFO);\
	  for n in $(REAL_OSM_MODELS); \
	  do \
	    fw=$${n}_$(OSM_GIT_TAG).bin; \
	    cp -v $(OSM_BUILD_DIR)/$$n/complete.bin $(OSM_FW_VERSION_DIR)/$$fw; \
	    echo "  {\"tag\": \"$(OSM_GIT_TAG)\", \"sha\": \"$(OSM_GIT_SHA1)\", \"path\": \"$${fw}\"}," >> $(OSM_FW_VERSION_INFO); \
	  done; \
	  echo "]" >> $(OSM_FW_VERSION_INFO); \
	  touch $@; \
	  tar Jcf $@ $(OSM_FW_VERSION_DIR); \
	else \
	  echo "No git tag for release.";\
	fi

fw_releases: $(OSM_FW_VERSION_TAR)

osm_all: $(OSM_MODELS_FW)
	@if [ $(OSM_GIT_SHA1_LEN) != 7 ]; then \
		echo "Error: git sha is incorrect length."; \
	fi

$(OSM_BUILD_DIR)/.git.$(OSM_GIT_COMMIT): $(LIBOPENCM3)
	mkdir -p "$(@D)"
	rm -f $(OSM_BUILD_DIR)/.git.*
	touch $@


include $(OSM_DIR)/ports/base.mk
include $(OSM_DIR)/ports/stm/stm.mk
include $(OSM_DIR)/ports/linux/linux.mk
include $(OSM_DIR)/ports/esp32/esp32.mk

define OSM_PROGRAM_template
  include $(1)
endef

OSM_PROGRAMS_MKS = $(shell find $(OSM_MODEL_DIR) -maxdepth 2 -name "*.mk" -printf "%p\n")

# To see what it's doing, just replace "eval" with "info"
$(foreach file_mk,$(OSM_PROGRAMS_MKS),$(eval $(call OSM_PROGRAM_template,$(file_mk))))

cppcheck: $(OSM_MODELS_CPPCHECK)


clean:
	make -C $(OSM_DIR)/libs/libopencm3 $@ TARGETS=stm32/l4
	rm -rf $(OSM_BUILD_DIR)

-include $(shell find "$(OSM_BUILD_DIR)" -name "*.d")

include $(OSM_DIR)/tests/tests.mk
