#!/bin/bash
# Uninstalls everything the DataCogs Plugins installer placed on this Mac.
#
# Removes the system-domain plugin bundles and (optionally) the system IR
# library, then forgets the package receipts. Never touches your personal
# IR library (~/Library/Audio/Impulse Responses/DataCogs), favourites, or
# any per-user plugin builds in ~/Library/Audio/Plug-Ins.
#
# Usage:
#   sudo packaging/uninstall.sh              # plugins only
#   sudo packaging/uninstall.sh --with-irs   # plugins + system IR library
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
  echo "error: needs sudo (installs live in /Library)" >&2
  exit 1
fi

WITH_IRS=0
[[ "${1:-}" == "--with-irs" ]] && WITH_IRS=1

removed=0
remove() {
  if [[ -e "$1" ]]; then
    rm -rf "$1"
    echo "removed: $1"
    removed=$((removed + 1))
  fi
}

for name in "DataCogs Compressor" "DataCogs EQ" "DataCogs Reverb"; do
  remove "/Library/Audio/Plug-Ins/Components/${name}.component"
  remove "/Library/Audio/Plug-Ins/VST3/${name}.vst3"
  remove "/Library/Application Support/Avid/Audio/Plug-Ins/${name}.aaxplugin"
done

if [[ $WITH_IRS -eq 1 ]]; then
  remove "/Library/Audio/Impulse Responses/DataCogs"
else
  echo "kept: /Library/Audio/Impulse Responses/DataCogs (pass --with-irs to remove)"
fi

for receipt in com.datacogs.plugins.suite com.datacogs.plugins.irlibrary; do
  pkgutil --forget "$receipt" > /dev/null 2>&1 && echo "forgot receipt: $receipt" || true
done

echo "done ($removed items removed). DAWs will drop the entries on their next plugin scan."
