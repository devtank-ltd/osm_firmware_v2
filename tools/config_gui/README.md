# OSM Configuration GUI

Run GUI
=====

To start the GUI, execute the following in the command line:

-------

    ./config_gui.py


Structure
====

At the top level directory, you will find the configuration GUI and it's modules such as
gui_binding_interface.py, modbus_db.py, modbus_funcs.py, config_gui_modbus.py, firmware_update.py and binding.py.

Images used by the GUI are stored in the directory osm_pictures.

Yaml files which are used to temporarily store Modbus device and register configuration are stored in the directory
yaml_files.

The database for storing Modbus configuration is located in config_database.

Firmware images for the OSM are stored in the directory firmware_imgs.

Scripts which can be executed on Linux machines are stored in the linux_os directory.

Scripts to create standalone bundles for Mac OS are stored in mac_os and in windows_os for Windows users.

Manuals and documentation are stored in docs.
