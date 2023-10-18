#!/bin/bash

echo "[SETUP] Install ESP32-ADF"
git clone --recursive https://github.com/espressif/esp-adf.git || exit 0

cd esp-adf
export ADF_PATH=$PWD

cd ${ADF_PATH}/esp-idf
git apply ../../patches/malloc.diff
./install.sh

. ./export.sh

