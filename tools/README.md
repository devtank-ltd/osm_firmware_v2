# Tools for Devtank OpenSmartMonitor

---

## 1. Config Generator

In this folder there is a config generator for the OpenSmartMonitor. This program will convert a .json file into a .img file to be later written to the device.

### Dependencies:
- libjson-c-dev

### Building

Type `make` while in this directory.

### Usage

1. A template configuration has been supplied with the name `memory_template.json`, copy this and modify with desired settings.

2. To generate the image: `./build/json_x_img /tmp/my_osm_config.img < JSON`

3. Now use the supplied script (in `PROJECT_DIR/scripts/`) to load this config onto the sensor.
