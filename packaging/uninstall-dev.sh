#!/bin/bash
# Removes the DEV builds that COPY_PLUGIN_AFTER_BUILD installs on every
# local build - the counterpart of uninstall.sh, which removes what the
# suite *installer* placed.
#
# Dev locations differ from the installer's: AU/VST3 go to the per-user
# folders (~/Library), while AAX always goes to the shared Avid folder
# (the same place the installer uses - whoever wrote last owns it).
# Your personal IR library and favourites are never touched.
#
# Usage: packaging/uninstall-dev.sh
# (no sudo needed if the Avid folder is user-writable, as a dev machine's
#  should be; otherwise rerun with sudo for the AAX bundles)
set -euo pipefail

removed=0
remove() {
  if [[ -e "$1" ]]; then
    if rm -rf "$1" 2>/dev/null; then
      echo "removed: $1"
      removed=$((removed + 1))
    else
      echo "PERMISSION DENIED (rerun with sudo): $1" >&2
    fi
  fi
}

for name in "DataCogs Compressor" "DataCogs EQ" "DataCogs Reverb"; do
  remove "$HOME/Library/Audio/Plug-Ins/Components/${name}.component"
  remove "$HOME/Library/Audio/Plug-Ins/VST3/${name}.vst3"
  remove "/Library/Application Support/Avid/Audio/Plug-Ins/${name}.aaxplugin"
done

echo "done ($removed items removed). The next local build reinstalls them all."
