
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

