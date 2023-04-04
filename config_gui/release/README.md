# OSM Configuration GUI

Use this application to configure your Open Smart Monitor with modbus and environmental measurements. Read data from it and create/edit modbus templates.

## Dependencies

To install all of the Python modules required, run the following commands:

```
pip install pyserial
pip install pyyaml
pip install pillow
pip install matplotlib
pip install pyinstaller
```

## Other modules and files

config_gui.py is the main application, run `python config_gui.py` to start the GUI.

modbus_func.py contains all of the code used by the 'Modbus Configuration' tab.

modbus_db.py contains the code which creates the sqlite3 database and handles the database queries.

binding.py contains the serial device object and the functions to transmit and receive data to it.

gui_binding_interface.py is the interface between the GUI and the binding.

complete.bin contains the firmware image used to flash the sensor.

config\_database is a directory containing the modbus templates database.

yaml\_files is the directory containing the temporary files which store the changes made to modbus templates.

stm32flash is the static executable used to apply the firmware image to the sensorsudo a

To create a static executable stm32 flasher, execute the `create_static_stm.sh` script.

This will check if the stm32flash directory exists, if not it will download and extract it. It will then remove the stm32flasher executable if it exists and create a new static executable and copy it into the release folder ready to use to flash the sensor.

## OSM Configuration GUI Download

If you have downloaded your own copy DO NOT rearrange the file structure as the program will not function properly.

To run the GUI, double click `OSM_Configuration_Gui`.

For a guide on how to use the GUI, `config_gui_manual.pdf` has been provided in the bundle.
