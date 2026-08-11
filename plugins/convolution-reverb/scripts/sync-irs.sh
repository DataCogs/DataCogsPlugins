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
gcloud storage rsync --recursive "${EXTRA[@]}" "$SRC" "$BUCKET"
echo "Done."
