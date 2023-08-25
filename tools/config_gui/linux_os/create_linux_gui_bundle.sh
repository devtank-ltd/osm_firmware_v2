#! /bin/bash

own_dir=$(dirname "$(readlink -f "$0")")
cd "$own_dir"

if [ -z "$(which pyinstaller)" ]; then echo "No pyinstaller installed"; exit 1; fi
if [ -z "$(which patchelf)" ]; then echo "No patchelf installed"; exit 1; fi
if [ -z "$(which staticx)" ]; then echo "No staticx installed"; exit 1; fi

if [ ! -e ../stm32flash-*/stm32flash ]
then
  ./create_static_stm.sh
fi

if [ ! -e ../stm32flash ]
then
  cp ../stm32flash-*/stm32flash ../stm32flash
fi

pyinstaller --onefile ../config_gui.py -n OSM\ Configuration\ GUI --hidden-import='PIL._tkinter_finder' --distpath .

staticx OSM\ Configuration\ GUI OSM\ Configuration\ GUI

mkdir -p osm_gui_bundle

cp -r OSM\ Configuration\ GUI ../config_database/ ../../../build/env01c/complete.bin ../stm32flash ../yaml_files/ ../osm_pictures/ ../docs/config_gui_manual.pdf osm_gui_bundle

tar -czvf /tmp/osm_gui_bundle.tar.gz osm_gui_bundle

rm -r OSM\ Configuration\ GUI OSM\ Configuration\ GUI.spec osm_gui_bundle build

echo "==================================="
echo "Bundle : /tmp/osm_gui_bundle.tar.gz"
echo "==================================="
