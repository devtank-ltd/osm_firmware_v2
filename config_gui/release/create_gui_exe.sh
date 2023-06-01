#! /bin/bash

own_dir=$(dirname "$(readlink -f "$0")")
cd "$own_dir"

if [ -z "$(which pyinstaller)" ]; then echo "No pyinstaller installed"; exit 1; fi
if [ -z "$(which patchelf)" ]; then echo "No patchelf installed"; exit 1; fi
if [ -z "$(which staticx)" ]; then echo "No static installed"; exit 1; fi

if [ ! -e ../stm32flash-*/stm32flash ]
then
  ../create_static_stm.sh
fi

if [ ! -e stm32flash ]
then
  cp ../stm32flash-*/stm32flash stm32flash
fi

pyinstaller --onefile config_gui.py --hidden-import='PIL._tkinter_finder' --distpath .

staticx config_gui config_gui

tar -czvf /tmp/osm_gui_bundle.tar.gz config_gui config_database/ complete.bin stm32flash yaml_files/ osm_pictures/ config_gui_manual.pdf ../README.md

echo "==================================="
echo "Bundle : /tmp/osm_gui_bundle.tar.gz"
echo "==================================="
