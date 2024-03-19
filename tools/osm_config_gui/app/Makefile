APP_DIR ?= .
RELEASE_DIR ?= fw_releases
OSM_DIR ?= $(APP_DIR)/../../..
GIT_TAG ?= $(shell git describe --tags --abbrev=0)
GIT_SHA ?= $(shell printf "%.*s\n" 7 $(shell git log -n 1 --format="%H"))
FW_PATH ?= $(RELEASE_DIR)/$(GIT_TAG).bin

VERSION_INFO := $(shell echo "{\"tag\": \"$(GIT_TAG)\", \"sha\": \"$(GIT_SHA)\", \"path\": \"$(FW_PATH)\"}" > $(RELEASE_DIR)/latest_fw_info.json)

default: all

all:
	mkdir -p $(RELEASE_DIR)
	$(VERSION_INFO)
	cp $(OSM_DIR)/build/env01d_lw/complete.bin $(RELEASE_DIR)/$(GIT_TAG).bin

clean:
	rm $(RELEASE_DIR)/latest_fw_info.json
	rm $(RELEASE_DIR)/$(GIT_TAG).bin
