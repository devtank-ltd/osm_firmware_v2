#! /bin/bash

. "$(dirname "$0")"/boot_mode.sh

stm32flash -b 115200 -i $gpio_str -S 0x0807f000 -w $1 /dev/$dev
