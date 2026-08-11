#!/bin/bash
# Pushes the local IR library (new IRs, photos, favourites) up to the
# private GCS bucket. Non-destructive: nothing is deleted from the bucket;
# to remove something everywhere, delete it locally and run with --delete.
set -euo pipefail

BUCKET="gs://datacogs-ir-library/ir-library"

case "$(uname)" in
  Darwin) SRC="$HOME/Library/Audio/Impulse Responses/DataCogs" ;;
  *)      SRC="$HOME/Documents/DataCogs/Impulse Responses" ;;
esac

EXTRA=()
if [[ "${1:-}" == "--delete" ]]; then
  EXTRA+=(--delete-unmatched-destination-objects)
  echo "Mirroring (bucket files missing locally WILL be deleted)."
fi

echo "Syncing IR library: $SRC -> $BUCKET"
# (bash 3.2 on macOS: an empty array counts as unbound under set -u)
gcloud storage rsync --recursive ${EXTRA[@]+"${EXTRA[@]}"} "$SRC" "$BUCKET"
echo "Done."
