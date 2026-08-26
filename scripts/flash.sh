#!/usr/bin/env sh
set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
  echo "Usage: $0 PORT [release-binary]" >&2
  exit 2
fi

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
port=$1
binary=${2:-"$repo_root/release/esp8266-serial-wifi-bridge-v0.3.0-rc1.bin"}
release_root="$repo_root/release/"
fqbn='esp8266:esp8266:generic:eesz=4M,FlashMode=dout,FlashFreq=40,CrystalFreq=26,wipe=none'

case "$binary" in
  /*) ;;
  *) binary="$repo_root/$binary" ;;
esac
case "$binary" in
  "$release_root"*) ;;
  *) echo 'For safety, the binary must be under release/.' >&2; exit 2 ;;
esac

command -v arduino-cli >/dev/null 2>&1 || {
  echo 'arduino-cli is required.' >&2
  exit 2
}
"$repo_root/scripts/verify-release.sh"

echo "Port:   $port"
echo "Image:  $binary"
echo 'Target: ESP8266, 4 MB, DOUT, 40 MHz, 26 MHz crystal'
echo 'Erase:  sketch/application sectors only; no full-chip erase requested'
printf 'Type FLASH to continue: '
read -r answer
[ "$answer" = 'FLASH' ] || { echo 'Cancelled; nothing was written.'; exit 2; }

arduino-cli upload --port "$port" --fqbn "$fqbn" --input-file "$binary" --verify
echo 'UPLOAD VERIFIED.'
