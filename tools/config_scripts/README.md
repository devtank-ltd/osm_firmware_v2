# Scripts for Devtank OpenSmartMonitor

---

## 1. Config Wipe

This is a script to wipe the persistent memory storing the configuration of the smart monitor (the firmware will remain unchanged).

### Usage

1. `./PROJECT_DIR/config_wipe.sh`

## 2. Config Save

This is a script to save the persistent memory storing the configuration of the smart monitor (the firmware will remain unchanged).

### Usage

1. Type `./PROJECT_DIR/config_save.sh IMG_LOCATION` with `IMG_LOCATION` being the place to save the image of the config.

2. To convert this image to a readable format use a the Config generator tool supplied.

## 3. Config Load

This is a script to load an image of a configuration onto the smart monitor (the firmware will remain unchanged).

### Usage

1. Type `./PROJECT_DIR/config_load.sh IMG_LOCATION` with `IMG_LOCATION` being the location of the .img file.


## 4. Program

This is a script to write a firmware image onto the smart monitor.

### Usage

1. Type `./PROJECT_DIR/program.sh FW_BIN` with `FW_BIN` being the location of the firmware .bin file.

2. Set the environment variable `KEEPCONFIG=1` to prevent erasing your config. WARNING - Config may still get erased depending on the firmware version, it is advisable to save your config before flashing, see 'Config Save'.
