Devtank OpenSmartMonitor
========================

This is a firmware for Devtank's OpenSmartMonitor based round the STML4.

The build environment is Debian with the packages:

make
gcc-arm-none-eabi
picolibc-arm-none-eabi
stm32flash

To program a OpenSmartMonitor you're need either "stm32flash" or "openocd" and to have soldered wired on the right pins....

This is all still work in progress and inherits code from previous Devtank STM projects.


Flash on to device
==================

The easiest way is with stm32flash.

Once built you should be able to do:

    sudo make serial_program

If the device has a STlink v2.1 debugger attached you can program with:

    make flash


Debugging
=========

For a STlink v2.1 debugger there are some make rules setting up debugging.
You need GNU Screen or two terminals.

In the first do:

    make debug_mon

In the second do:

    make debug_gdb


Or connect GNU GDB to OpenOCD's remote GDB localhost:3333 however you like. 


License
=======

Please see License file.
