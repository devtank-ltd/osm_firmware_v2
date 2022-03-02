#! /bin/bash

. "$(dirname "$0")"/boot_mode.sh

stm32flash -b 115200 -i $gpio_str -s $1 -w "$2" /dev/$dev
