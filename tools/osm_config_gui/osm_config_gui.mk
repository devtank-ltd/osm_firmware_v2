
GIT_TAG2 := $(shell git describe --tags --abbrev=0 --dirty)

WEBSERVE_DIR := $(OSM_DIR)/tools/osm_config_gui/app

WEBROOT_BUILD_DIR := $(BUILD_DIR)/osm_config_gui/webroot
WEBROOT_LIB_BUILD_DIR := $(WEBROOT_BUILD_DIR)/libs

define WEBSERVE_BUILT_FILES
$(1)_SRC := $(shell find $(2) -type f)
$(1) := $$(shell for x in $$($(1)_SRC); do echo $$(WEBROOT_BUILD_DIR)/`realpath --relative-to="$$(WEBSERVE_DIR)" "$$$${x}"`; done)
$(1): $$(WEBROOT_BUILD_DIR)/%: $$($(2))/%
	@mkdir -p $$(@D)
	cp $$< $$@
endef

$(eval $(call WEBSERVE_BUILT_FILES,WEBSERVE_GUI,$(WEBSERVE_DIR)/modules/gui))
$(eval $(call WEBSERVE_BUILT_FILES,WEBSERVE_BACKEND,$(WEBSERVE_DIR)/modules/backend))
$(eval $(call WEBSERVE_BUILT_FILES,WEBSERVE_IMG,$(WEBSERVE_DIR)/imgs))
$(eval $(call WEBSERVE_BUILT_FILES,WEBSERVE_STYLES,$(WEBSERVE_DIR)/styles))

FW_VERSION_INFO := $(WEBROOT_BUILD_DIR)/fw_releases/latest_fw_info.json

webroot: $(WEBROOT_BUILD_DIR)/index.html $(WEBROOT_BUILD_DIR)/favicon.ico $(WEBSERVE_GUI) $(WEBSERVE_BACKEND) $(WEBSERVE_IMG) $(WEBSERVE_STYLES) $(BUILD_DIR)/.webroot/libs $(BUILD_DIR)/osm_config_gui/aioserver.py $(BUILD_DIR)/.webroot/fw_releases

.PHONY: webhost

webhost: webroot
	cd $(BUILD_DIR)/osm_config_gui; \
	python3 ./aioserver.py -v

$(WEBROOT_BUILD_DIR)/%: $(WEBSERVE_DIR)/%
	@mkdir -p $(@D)
	cp $< $@

$(BUILD_DIR)/osm_config_gui/aioserver.py: $(WEBSERVE_DIR)/aioserver.py
	@mkdir -p $(@D)
	cp $< $@

$(BUILD_DIR)/.webroot/libs: $(BUILD_DIR)/.webroot/lib_stm32flash
	@mkdir -p $(@D)
	touch $@

$(BUILD_DIR)/.webroot/lib_stm32flash:
	@mkdir -p $(WEBROOT_LIB_BUILD_DIR)/stm-serial-flasher/
	rsync -ar --include='*.js' --exclude='*.*' $(WEBSERVE_DIR)/modules/stm-serial-flasher/src/ $(WEBROOT_LIB_BUILD_DIR)/stm-serial-flasher
	@mkdir -p $(@D)
	touch $@

$(BUILD_DIR)/.webroot/fw_releases: $(REAL_MODELS)
	$(shell mkdir -p $(WEBROOT_BUILD_DIR)/fw_releases; \
	touch $(FW_VERSION_INFO); \
	echo "[" > $(FW_VERSION_INFO); \
	for x in $^; do \
	    cp $(BUILD_DIR)/$${x}/firmware.bin $(WEBROOT_BUILD_DIR)/fw_releases/$${x}_release.bin; \
	    echo "  {\"tag\": \"$(GIT_TAG2)\", \"sha\": \"$(GIT_SHA1)\", \"path\": \"$${x}_release.bin\"}," >> $(FW_VERSION_INFO); \
	done; \
	truncate -s -2 $(FW_VERSION_INFO); \
	echo "\n]" >> $(FW_VERSION_INFO);)
	@mkdir -p $(@D)
	touch $@
