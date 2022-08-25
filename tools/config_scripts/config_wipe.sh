#! /bin/bash

. "$(dirname "$0")"/boot_mode.sh

stm32flash -b 115200 -i $gpio_str -S ${config_addr}:${config_size} -o /dev/$dev
