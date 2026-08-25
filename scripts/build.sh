#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_dir=${1:-"$repo_root/build"}
fqbn='esp8266:esp8266:generic:eesz=4M,FlashMode=dout,FlashFreq=40,CrystalFreq=26,wipe=none'
package_url='https://arduino.esp8266.com/stable/package_esp8266com_index.json'
sketch="$repo_root/firmware/esp8266-serial-wifi-bridge"

command -v arduino-cli >/dev/null 2>&1 || {
  echo 'arduino-cli is required: https://arduino.github.io/arduino-cli/latest/installation/' >&2
  exit 2
}

arduino-cli config add board_manager.additional_urls "$package_url" >/dev/null
arduino-cli core update-index
arduino-cli core install esp8266:esp8266@3.1.2
mkdir -p "$output_dir"
arduino-cli compile --fqbn "$fqbn" --output-dir "$output_dir" "$sketch"

image="$output_dir/esp8266-serial-wifi-bridge.ino.bin"
echo "Built: $image"
if command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$image"
else
  shasum -a 256 "$image"
fi
