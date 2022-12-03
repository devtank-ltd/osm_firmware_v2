OSM_DIR ?= .
BUILD_DIR ?= $(OSM_DIR)/build
MODEL_DIR ?= $(OSM_DIR)/model

GIT_COMMITS := $(shell git rev-list --count HEAD)
GIT_COMMIT := $(shell git log -n 1 --format="%h-%f")
GIT_SHA1 := $(shell git log -n 1 --format="%h")
GIT_TAG ?= $(shell git tag --points-at HEAD)

MODELS = $(shell find $(MODEL_DIR)/* -type d -printf '%f\n')
MODELS_FW = $(MODELS:%=$(BUILD_DIR)/%/.complete)
MODELS_CPPCHECK = $(MODELS:%=%_cppcheck)

RELEASE_DIR ?= $(BUILD_DIR)/releases


RELEASE_NAME := $(GIT_TAG)_release_bundle

JSON_CONV := $(BUILD_DIR)/tool/json_x_img

all: $(MODELS_FW)

$(BUILD_DIR)/.git.$(GIT_COMMIT): $(LIBOPENCM3)
	mkdir -p "$(@D)"
	rm -f $(BUILD_DIR)/.git.*
	touch $@


define PORT_BASE_OBJ_RULE
	mkdir -p "$$(@D)"
	$$($(2)_CC) -c -Dfw_name=$(1) -DFW_NAME=$$($(1)_UP_NAME) $$($(2)_CFLAGS) -I$$(MODEL_DIR)/$(1) $$($(2)_INCLUDE_PATHS) $$< -o $$@
endef

define PORT_BASE_RULES
$(1)_UP_NAME=$$(shell echo $(1) | tr a-z A-Z)

$(1)_IOSM_SRC=$$(filter-out $$(MODEL_DIR)/%,$$($(1)_SOURCES))
$(1)_NOSM_SRC=$$(filter $$(MODEL_DIR)/%,$$($(1)_SOURCES))

$(1)_IOSM_OBJS=$$($(1)_IOSM_SRC:$$(OSM_DIR)/%.c=$$(BUILD_DIR)/$(1)/%.o)

$(1)_NOSM_OBJS=$$($(1)_NOSM_SRC:$$(MODEL_DIR)/%.c=$$(BUILD_DIR)/$(1)/%.o)

$(1)_OBJS:= $$($(1)_IOSM_OBJS) $$($(1)_NOSM_OBJS)


$$($(1)_IOSM_OBJS): $$(BUILD_DIR)/$(1)/%.o: $$(OSM_DIR)/%.c $$(BUILD_DIR)/.git.$$(GIT_COMMIT)
$(call PORT_BASE_OBJ_RULE,$(1),$(2))

$$($(1)_NOSM_OBJS): $$(BUILD_DIR)/$(1)/%.o: $$(MODEL_DIR)/%.c $$(BUILD_DIR)/.git.$$(GIT_COMMIT)
$(call PORT_BASE_OBJ_RULE,$(1),$(2))

$(1)_cppcheck:
	@echo ================
	@echo cppcheck $(1)
	cppcheck --std=c11 $$($(2)_DEFINES) $$($(1)_SOURCES) -I$$(MODEL_DIR)/$(1) $$($(2)_INCLUDE_PATHS)

endef


include $(OSM_DIR)/ports/stm.mk
include $(OSM_DIR)/ports/linux.mk
include $(OSM_DIR)/tools/img_json_interpretter/tool.mk

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
