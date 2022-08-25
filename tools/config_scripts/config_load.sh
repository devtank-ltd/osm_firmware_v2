#! /bin/bash

. "$(dirname "$0")"/boot_mode.sh

stm32flash -b 115200 -i $gpio_str -s 254 -w $1 /dev/$dev
