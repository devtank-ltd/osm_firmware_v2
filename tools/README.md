# Tools for Devtank OpenSmartMonitor

## 1. JSON Interpretter

In the `img_json_interpretter` directory there is a config generator for the OpenSmartMonitor.
This program will convert a .json file into a .img file to be later written to the device.

### Dependencies:
- libjson-c-dev

### Building

1. Enter into the directory with `cd json_img_interpretter`.

2. Type `make` while in this directory.

### Usage

1. A template configuration has been supplied with the name `memory_template.json`, copy this and modify with desired settings.

2. To generate the image: `./build/json_x_img /tmp/my_osm_config.img < JSON`

3. Now use the supplied script (in `PROJECT_DIR/scripts/`) to load this config onto the sensor.


## 2. Configuration Scripts

There is also a file for configuration scripts in the `config_scripts` directory.
These are to manipulate the persistent memory on the OSM Sensor.
There are three main functions of this: save a config, load a config and wipe a config.

Consult `config_scripts/README.md` for usage.
