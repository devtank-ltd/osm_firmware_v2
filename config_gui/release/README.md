-----OSM Configuration GUI-----

Use this application to configure your Open Smart Monitor with modbus and environmental measurements. Read data from it and create/edit modbus templates.

config\_gui.py contains the gui and all its functions, run this program to start the app.
modbus\_func.py contains functions for the modbus configuration tab.
modbus\_db.py contains the sqlite3 database and all of its functions.
binding.py contains functions required to gather data structures for modbus, ios etc.

complete.bin is the firmware image used to flash the sensor.

config\_database is a directory containing the modbus templates database.

yaml\_files is the directory containing the temporary files which store the changes made to modbus templates.

stm32flash is the static executable used to apply the firmware image to the sensor

To create a static executable stm32 flasher

cd into config\_gui directory

Run "./create\_static\_stm.sh"

This will check if the stm32flash directory exists, if not it will download and extract it. It will then remove the stm32flasher executable if it exists and create a new static executable and copy it into the release folder ready to use to flash the sensor.

----OSM Configuration GUI Download-----

If you have downloaded your own copy DO NOT rearrange the file structure as the program will not function properly.

To run the GUI, double click OSM\_Configuration\_Gui.

For a guide on how to use the GUI, 'config\_gui\_manual.pdf' has been provided in the bundle.
