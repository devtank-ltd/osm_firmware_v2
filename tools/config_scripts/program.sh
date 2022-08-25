#! /bin/bash

. "$(dirname "$0")"/boot_mode.sh

bootloader="bootloader.bin"
firmware="firmware.bin"

cleanup()
{
  echo "Failed to write the firmware."
  rm ${bootloader} ${firmware}
  exit -1
}


trap 'cleanup' ERR


if [[ -n "$KEEPCONFIG" ]]
then
  dd of=${bootloader} if=$1 count=2 bs=2k
  dd of=${firmware} if=$1 skip=4 bs=2k
  stm32flash -b 115200 -i $gpio_str -S ${bootloader_addr}:${bootloader_size} -w ${bootloader} /dev/$dev
  stm32flash -b 115200 -i $gpio_str -S ${firmware_addr} -w ${firmware}   /dev/$dev
  rm ${bootloader} ${firmware}
else
  stm32flash -b 115200 -i $gpio_str -w $1 /dev/$dev
fi

