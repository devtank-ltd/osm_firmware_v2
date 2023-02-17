#! /bin/bash

if [[ -z "$dev" ]]
then
  dev=$(basename $(readlink /dev/serial/by-id/*CP210*))
fi

echo Using /dev/$dev

users=$(fuser /dev/$dev 2>/dev/null)

if [ -n "$users" ]
then
  echo "ERROR: /dev/$dev is in use!"
  exit -1
fi

if [[ -n "$REVC" ]]
then
  boot="rts"
  reset="dtr"

  gpio_str="$boot&-$reset,,$reset,,:-$reset,-$boot,,,"
else
  gpiochip=$(ls /sys/class/tty/$dev/device/../gpio/)

  gpiobase=${gpiochip/gpiochip}
  boot=$(($gpiobase+3))
  reset=$(($gpiobase+2))

  gpio_str="-$boot&-$reset,,$reset,,:$boot&-$reset,,$reset"
fi

#entry sequence: GPIO_3=low, GPIO_2=low, 100ms delay, GPIO_2=high
#exit sequence: GPIO_3=high, GPIO_2=low, 300ms delay, GPIO_2=high
#./stm32flash -i '-3&-2,2:3&-2,,,2' /dev/ttyS0

bootloader_addr=0x08000000
bootloader_size=0x00001000
config_addr=0x08001000
config_size=0x00001000
firmware_addr=0x08002000
