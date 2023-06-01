#! /bin/bash

KEEPCONFIG=1

bootloader="bootloader.bin"
firmware="firmware.bin"

if [[ -z "$dev" ]]
then
  export dev=$(basename $(readlink /dev/serial/by-id/*CP210*))
fi
echo Using /dev/$dev

users=$(fuser /dev/$dev 2>/dev/null)

cleanup()
{
  echo "Failed to write the firmware."
  rm ${bootloader} ${firmware}
  exit -1
}


trap 'cleanup' ERR

gpiochip=$(ls /sys/class/tty/$dev/device/../gpio/)

gpiobase=${gpiochip/gpiochip}
boot=$(($gpiobase+3))
reset=$(($gpiobase+2))

export gpio_str="-$boot&-$reset,,$reset,,:$boot&-$reset,,$reset"

#entry sequence: GPIO_3=low, GPIO_2=low, 100ms delay, GPIO_2=high
#exit sequence: GPIO_3=high, GPIO_2=low, 300ms delay, GPIO_2=high

bootloader_addr=0x08000000
bootloader_size=0x00001000
config_addr=0x08001000
config_size=0x00001000
firmware_addr=0x08002000


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

