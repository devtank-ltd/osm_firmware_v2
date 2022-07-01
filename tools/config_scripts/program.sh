#! /bin/bash

. "$(dirname "$0")"/boot_mode.sh

if [[ -n "$KEEPCONFIG" ]]
then
  arg="-e 254"
fi

stm32flash -b 115200 -i $gpio_str $arg -w $1 /dev/$dev
