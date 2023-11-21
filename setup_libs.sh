#!/bin/bash

echo "[SETUP] Install ESP32-ADF"
git clone --recursive https://github.com/espressif/esp-adf.git || exit 0

echo "[SETUP] Install ESP32-IDF"
git clone --recursive https://github.com/espressif/esp-idf.git || exit 0

export ADF_PATH=$PWD/esp-adf
export IDF_PATH=$PWD/esp-idf

cd ${IDF_PATH}
git apply ../patches/malloc.diff
./install.sh

. ./export.sh

