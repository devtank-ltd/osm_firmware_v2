#! /bin/bash

pyinstaller --onefile config_gui.py --hidden-import='PIL._tkinter_finder' 

mv dist/config_gui .

staticx config_gui OSM_Configuration_Gui

 tar -czvf osm_gui_bundle.tar.gz OSM_Configuration_Gui config_database/ complete.bin stm32flash yaml_files/ osm_pictures/ ../config_gui_manual.pdf ../README.md ../create_static_stm.sh static_program.sh

cp osm_gui_bundle.tar.gz ~/config_gui_bundle/

