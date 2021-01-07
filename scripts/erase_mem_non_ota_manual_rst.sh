# Please update parameters below in accordance with your TTY\Pip\Python\EspSDK configuration
PORT=/dev/serial/esp8266
PYTHON=/usr/bin/python2.7
ESP_TOOL=~/.local/bin/esptool.py
FW_BIN_DIR=~/esp8266-dev-kits/esp-native-sdk/bin

if [ ! -c $PORT ]; then
  echo "ERROR: No USB tty device found"
  exit 1
else
  echo "INFO: Using USB tty device: $PORT"
fi

echo
echo "INFO: Erasing Flash Memory... "

$PYTHON $ESP_TOOL --chip esp8266 --port $PORT --baud 115200 --before no_reset --after no_reset erase_flash

if [ $? -eq "0" ]; then
  echo "INFO: Done Erasing. Please reset your ESP module before the next step."
else
  echo "ERROR: Unable to complete \"Erase Flash Memory\" step"
  exit 1
fi
