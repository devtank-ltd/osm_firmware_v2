# Getting Started With OpenSmartMonitor

Setup
=====

To clone the repository, execute the following command in your desired
directory.

-------

    git clone --recursive https://git.devtank.co.uk/Devtank/osm_firmware.git
    git pull
    git submodule init
    git submodule update

There are several dependencies required for OSM to compile successfully,install these on your machine with the following command:

    sudo apt install
        gcc-arm-none-eabi \
        build-essential \
        git \
        pkg-config \
        libjson-c-dev \
        picolibc-arm-none-eabi \
        stm32flash \
        valgrind \
        minicom \
        idle3 \
        python3-influxdb \
        python3-pil.imagetk \
        python3-pymodbus \
        python3-scipy \
        python3-yaml \
        python3-crccheck \
        python3-paho-mqtt \
        nodejs

Build
=====

In order to build OSM, run the command:

-------

    make -j8

You can also run *make* but the above command will compile much faster.

Test
====

Now that osm\_firmware has compiled, we can begin running tests and
communicating with the fake osm. To run a test for the virtual OSM,
enter the following command in the top level directory.

-------

    make penguin_lw_test

This will spawn the virtual OSM test, connect to the virtual OSM and
test values for each measurement, ensure you wait for the measurement
loop to finish.

Run Virtual OSM
===

To spawn the Virtual OSM, use the following command:

-------

    ./build/penguin_lw/firmware.elf

Once this is running, you can use minicom to open up communications with
the fake sensor. The device that you will want to supply to minicom is
created when starting the Virtual OSM and is called
*UART\_DEBUG\_slave*.

-------

    minicom -b 115200 -D /tmp/osm/UART_DEBUG_slave

You can now communicate with the Virtual OSM through serial.


Text interface
==============

The OSM, virtual or real, offers a text interface over serial. Typing a "?" and carriage return will make it print out the available commands.

Taking a measurement manually
-----------------------------

The example here will be to take a measurement manually.

If comms (LoRaWAN/Wifi/PoE) has been established, the measurement loop will have been started.
Reading measurements manually can conflict with the measurement loop, so the first thing to do is to stop the measurement loop.

    meas_enable 0

Once the measurement loop is not running, and the state is clean, it is safe to take measurements.

Get the list of possible measurement with:

    measurements

When you have the measurement you want to take, you use the "get_meas" command to take the measurement.

For example, to take a noise level reading you would do:

    get_meas SND

