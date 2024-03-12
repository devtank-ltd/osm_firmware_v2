APP_DIR ?= .
RELEASE_DIR ?= $(APP_DIR)/fw_releases
OSM_DIR ?= $(APP_DIR)/../../..


VERSION_INFO := $(shell echo "{\"tag\": \"$$(git describe --tags --abbrev=0)\", \"sha\": \"$$(printf "%.*s\n" 7 $$(git log -n 1 --format="%H"))\"}" > version_info.txt)

default: all

all:
	$(VERSION_INFO)
	cp $(OSM_DIR)/build/env01d_lw/complete.bin $(RELEASE_DIR)/

clean:
	rm $(APP_DIR)/version_info.txt
	rm $(RELEASE_DIR)/complete.bin
