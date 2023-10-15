# TeddyBox

## Setup
Tried on linux (WSL, Ubuntu-22.04) only yet.
Execute ./setup_libs.sh which will download esp-adf and esp-idf.

## Environment
Before you can compile, you have to source the "env_vars.sh" in your shell environment.
To do so, enter
 . ./env_vars.sh
with the first dot in place.

## Compiling
Enter ./teddybox directory and enter
  idf.py build
Now the build system will build the binaries.
To flash follow the commandline output or use "make flash" which is hardcoded to a certain USB tty at the moment.
