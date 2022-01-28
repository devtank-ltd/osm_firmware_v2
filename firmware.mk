firmware_SOURCES := main.c \
           log.c \
           sai.c \
           uarts.c \
           uart_rings.c \
           cmd.c \
           io.c \
           ring.c \
           hpm.c \
           i2c.c \
           htu21d.c \
           modbus.c \
           value.c \
           persist_config.c \
           lorawan.c \
           measurements.c \
           modbus_measurements.c \
           update.c \
           adcs.c \
           one_wire_driver.c \
           timers.c

firmware_LINK_SCRIPT := stm32l452.ld