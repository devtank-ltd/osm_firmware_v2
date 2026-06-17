pp01a_SOURCES := \
    $(OSM_DIR)/src/core/main.c \
    $(OSM_DIR)/src/core/base.c \
    $(OSM_DIR)/src/core/cmd.c \
    $(OSM_DIR)/src/core/common.c \
    $(OSM_DIR)/src/core/log.c \
    $(OSM_DIR)/src/core/measurements.c \
    $(OSM_DIR)/src/core/measurements_mem.c \
    $(OSM_DIR)/src/core/persist_base.c \
    $(OSM_DIR)/src/core/platform_common.c \
    $(OSM_DIR)/src/core/ring.c \
    $(OSM_DIR)/src/core/uart_rings.c \
    $(OSM_DIR)/src/ports/rp2350/rp2350.c \
    $(OSM_DIR)/src/ports/rp2350/persist_config.c \
    $(OSM_DIR)/src/ports/rp2350/sleep.c \
    $(OSM_DIR)/src/ports/rp2350/uarts.c \
    $(OSM_DIR)/src/ports/rp2350/rp2350_comms.c \
    $(OSM_DIR)/src/ports/rp2350/update.c \
    $(OSM_DIR)/src/ports/rp2350/i2s.c \
    $(OSM_DIR)/src/protocols/jsonblob.c \
    $(OSM_DIR)/src/protocols/comms_behind.c \
    $(OSM_DIR)/src/sensors/fw.c \
    $(OSM_MODEL_DIR)/pp01a/main/model.c

$(eval $(call RP2350_FIRMWARE,pp01a))
