#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_root/release"
if command -v sha256sum >/dev/null 2>&1; then
  sha256sum -c SHA256SUMS.txt
else
  shasum -a 256 -c SHA256SUMS.txt
fi
