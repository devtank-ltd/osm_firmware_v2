#! /bin/bash

dev=$(basename $(readlink /dev/serial/by-id/*CP210*))

echo Using /dev/$dev

gpiochip=$(ls /sys/class/tty/$dev/device/../gpio/)

gpiobase=${gpiochip/gpiochip}
boot=$(($gpiobase+3))
reset=$(($gpiobase+2))

gpio_str="-$boot&-$reset,,$reset,,:$boot&-$reset,,$reset"

#entry sequence: GPIO_3=low, GPIO_2=low, 100ms delay, GPIO_2=high
#exit sequence: GPIO_3=high, GPIO_2=low, 300ms delay, GPIO_2=high
#./stm32flash -i '-3&-2,2:3&-2,,,2' /dev/ttyS0
