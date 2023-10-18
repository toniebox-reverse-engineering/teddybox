
DEVICE:=/dev/ttyS5

.PHONY: flash

flash:
	stty -F ${DEVICE} 921600 
	/root/.espressif/python_env/idf4.4_py3.10_env/bin/python ../esp-adf/esp-idf/components/esptool_py/esptool/esptool.py -p ${DEVICE} -b 921600 --before default_reset --after hard_reset --chip esp32s3  write_flash --flash_mode dio --flash_size detect --flash_freq 80m 0x180000 build/teddybox.bin
	idf.py monitor -p ${DEVICE}
