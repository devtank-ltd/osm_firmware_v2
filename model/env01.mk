env01_SOURCES := \
           $(OSM_DIR)/core/src/main.c \
           $(OSM_DIR)/core/src/base.c \
           $(OSM_DIR)/core/src/log.c \
           $(OSM_DIR)/core/src/uart_rings.c \
           $(OSM_DIR)/core/src/cmd.c \
           $(OSM_DIR)/core/src/io.c \
           $(OSM_DIR)/core/src/ring.c \
           $(OSM_DIR)/core/src/modbus_mem.c \
           $(OSM_DIR)/core/src/persist_config.c \
           $(OSM_DIR)/core/src/modbus.c \
           $(OSM_DIR)/core/src/measurements.c \
           $(OSM_DIR)/core/src/measurements_mem.c \
           $(OSM_DIR)/core/src/modbus_measurements.c \
           $(OSM_DIR)/core/src/update.c \
           $(OSM_DIR)/core/src/adcs.c \
           $(OSM_DIR)/core/src/common.c \
           $(OSM_DIR)/core/src/platform_common.c \
           $(OSM_DIR)/core/src/debug_mode.c \
           $(OSM_DIR)/protocols/src/hexblob.c \
           $(OSM_DIR)/comms/src/lw.c \
           $(OSM_DIR)/comms/src/rak4270.c \
           $(OSM_DIR)/sensors/src/hpm.c \
           $(OSM_DIR)/sensors/src/htu21d.c \
           $(OSM_DIR)/sensors/src/ds18b20.c \
           $(OSM_DIR)/sensors/src/pulsecount.c \
           $(OSM_DIR)/sensors/src/veml7700.c \
           $(OSM_DIR)/sensors/src/sai.c \
           $(OSM_DIR)/sensors/src/cc.c \
           $(OSM_DIR)/sensors/src/bat.c \
           $(OSM_DIR)/sensors/src/can_impl.c \
           $(OSM_DIR)/sensors/src/fw.c \
           $(OSM_DIR)/sensors/src/io_watch.c \
           $(MODEL_DIR)/env01/env01.c \
           $(OSM_DIR)/ports/stm/src/can_comm.c \
           $(OSM_DIR)/ports/stm/src/stm.c \
           $(OSM_DIR)/ports/stm/src/i2c.c \
           $(OSM_DIR)/ports/stm/src/sleep.c \
           $(OSM_DIR)/ports/stm/src/timers.c \
           $(OSM_DIR)/ports/stm/src/uarts.c \
           $(OSM_DIR)/ports/stm/src/w1.c

env01_LINK_SCRIPT := $(OSM_DIR)/ports/stm/stm32l4.ld

$(eval $(call STM_FIRMWARE,env01))
