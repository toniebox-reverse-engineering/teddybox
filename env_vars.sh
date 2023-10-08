#!/bin/bash

# Check if the script is sourced
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo "Error: This script should be sourced, not executed directly."
    echo "  Execute: '. ${0}' in your shell"
    exit 1
fi

echo "[ENV] Setting ADF/IDF paths"
export ADF_PATH=$PWD/esp-adf
export IDF_PATH=$ADF_PATH/esp-idf

echo "[ENV] Exporting IDF vars"
. $IDF_PATH/export.sh

echo "[ENV] Done"
