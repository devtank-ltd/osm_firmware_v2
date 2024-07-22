#! /bin/bash

if [ -z "$1" ]
then
  echo "Please provide config file to load on to OSM."
  exit -1
fi

. "$(dirname "$0")"/boot_mode.sh

stm32flash -b 115200 -i $gpio_str -S ${config_addr}:${config_size} -w $1 /dev/$dev
