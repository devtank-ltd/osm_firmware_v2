Devtank OpenSmartMonitor - STMs
===============================

The build environment for STM based OSM is Debian with the packages:

* make
* gcc-arm-none-eabi
* picolibc-arm-none-eabi
* stm32flash

To program a OpenSmartMonitor you're need either "stm32flash" or "openocd" and to have soldered wired on the right pins....

This is all still work in progress and inherits code from previous Devtank STM projects.


Flash on to device
==================

The easiest way is with stm32flash.

With <MODEL> being the model name, for example, env01, do:

    sudo make <MODEL>_serial_program

If the device has a STlink v2.1 debugger attached you can program with:

    make <MODEL>_flash


Debugging
=========

For a STlink v2.1 debugger there are some make rules setting up debugging.
You need GNU Screen or two terminals.

With <MODEL> being the model name, for example, env01, in the first do:

    make <MODEL>_debug_mon

In the second do:

    make <MODEL>_debug_gdb


Or connect GNU GDB to OpenOCD's remote GDB localhost:3333 however you like. 
