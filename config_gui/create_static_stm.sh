#!/bin/bash

[ -d "./stm32flash-0.6" ] || wget https://sourceforge.net/projects/stm32flash/files/stm32flash-0.6.tar.gz/download && tar -xvf download

rm download

cd stm32flash-0.6/

rm stm32flasher || echo "No stm32 flasher" && ./configure "LDFLAGS=--static" --disable-shared && make && cp -v stm32flash ../release/


