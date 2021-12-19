SOURCES += main.c \
           log.c \
           sai.c \
           uarts.c \
           uart_rings.c \
           cmd.c \
           io.c \
           ring.c \
           hpm.c \
           modbus.c \
           value.c \
           persist_config.c \
           lorawan.c \
           measurements.c \
           modbus_measurements.c

BUILD_DIR := build/
PROJECT_NAME := firmware

include Makefile.base
