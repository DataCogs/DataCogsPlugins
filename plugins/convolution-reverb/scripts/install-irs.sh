#!/bin/bash
# Installs the DataCogs IR library (IRs, photos, favourites, attribution)
# from the private GCS bucket onto this machine, into the folder the plugin
# scans. Non-destructive: files that exist only locally are left alone.
#
# Needs gcloud authenticated against the datacogs-dev project:
#   gcloud auth login && gcloud config set project datacogs-dev
set -euo pipefail

BUCKET="gs://datacogs-ir-library/ir-library"

case "$(uname)" in
  Darwin) DEST="$HOME/Library/Audio/Impulse Responses/DataCogs" ;;
  *)      DEST="$HOME/Documents/DataCogs/Impulse Responses" ;;
esac

mkdir -p "$DEST"
echo "Installing IR library: $BUCKET -> $DEST"
gcloud storage rsync --recursive "$BUCKET" "$DEST"
echo "Done. $(find "$DEST" -name '*.wav' | wc -l | tr -d ' ') IRs installed."
