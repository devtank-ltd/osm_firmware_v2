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


# There is an interaction between the dtr and rts, that means there is not a direct mapping to boot and reset.
# Here is the table:
# 00 : dtr Low   rts Low  - reset 1, boot 1
# 10 : dtr High  rts Low  - reset 0, boot 0
# 01 : dtr Low   rts High - reset 0, boot 1
# 11 : dtr High  rts High - reset 1, boot 0
# Represented by:
# BOOT = NOT DTR
# RESET = NOT (RTS XOR DTR)
# By default, rts and dtr are low.
# There is only one state where reset is low and boot is high, 01 (-dtr,rts)
# To program we go from 01 to 00, (-dtr,-rts)
# To return to normal 10 to 11 (dtr,rts)
gpio_str="-rts&-dtr,dtr,rts,:-dtr,-rts"

if [[ -n "$REVB" ]]
then
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
