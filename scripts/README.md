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
