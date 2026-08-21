#!/bin/bash
# Removes EVERY DataCogs plugin bundle on this Mac - both the dev builds
# that COPY_PLUGIN_AFTER_BUILD installs (uninstall-dev.sh) and whatever the
# suite installer placed in the system domain (uninstall.sh). Use it to get
# back to a known-clean slate before a rebuild or an installer test - e.g.
# when a root-owned AAX bundle from an old sudo install blocks the build's
# own copy/sign step.
#
# Never touches IR libraries or favourites unless asked: --with-irs is
# passed through to uninstall.sh and removes the SYSTEM IR library only
# (your personal one in ~/Library is always kept).
#
# Usage:
#   sudo packaging/uninstall-all.sh              # all plugin bundles
#   sudo packaging/uninstall-all.sh --with-irs   # + system IR library
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
  echo "error: needs sudo (system installs and root-owned bundles live in /Library)" >&2
  exit 1
fi

here="$(cd "$(dirname "$0")" && pwd)"

# Dev bundles live under the *invoking* user's ~/Library; running the dev
# script as root would look in root's home and miss them all.
dev_user="${SUDO_USER:-$(whoami)}"
sudo -u "$dev_user" HOME="$(dscl . -read "/Users/$dev_user" NFSHomeDirectory | awk '{print $2}')" \
  "$here/uninstall-dev.sh" || true
# Second pass as root for anything the user-level run couldn't delete
# (e.g. AAX bundles owned by root from an old sudo install).
"$here/uninstall-dev.sh"

"$here/uninstall.sh" "$@"

echo "all clean. The next local build reinstalls (and signs) the dev bundles."
