#!/bin/bash
# Builds the macOS suite installer: "DataCogs Plugins <version>.pkg".
#
# Two components, both default-on:
#   Plugins    - AU + VST3 + AAX for all three plugins
#                -> /Library/Audio/Plug-Ins/{Components,VST3}
#                -> /Library/Application Support/Avid/Audio/Plug-Ins
#   IR Library - impulse responses + artwork + attribution for the reverb
#                -> /Library/Audio/Impulse Responses/DataCogs
#                (the plugin scans this system root after the user's own
#                 library, so personal captures always win)
#
# AAX signing - two supported modes:
#   --sign-aax     wraptool-signs the staged AAX bundles right here. Works
#                  with a USB iLok plugged in or an open iLok Cloud session
#                  as-is. For headless CI (PACE cloud signing), export
#                  PACE_PASSWORD: the script opens an iLok Cloud session via
#                  iloktool and signs with --allowsigningservice, per PACE's
#                  "Code Signing of AAX plug-ins utilizing the iLok Cloud"
#                  guide. The password enters this script only via the
#                  environment (never a script argument, never logged); it
#                  is then passed to iloktool's --password flag - PACE's
#                  documented interface - so it is briefly visible in the
#                  process list of the (ephemeral) signing machine. If PACE
#                  adds an env/stdin option, switch to it. Note: one cloud
#                  session per machine - concurrent signers need separate
#                  iLok accounts.
#   (without it)   prefers the already-signed AAX installed in the Avid
#                  folder by a local dev build, and warns if it has to fall
#                  back to the unsigned build-tree copy.
#
# Usage:
#   packaging/build-installer.sh [--build-dir build] [--ir-library <path>]
#       [--out dist] [--version 0.1.0]
#       [--sign "Developer ID Installer: Name (TEAMID)"]
#       [--sign-aax] [--pace-account kogzee] [--pace-wcguid <guid>]
#       [--aax-signid "Developer ID Application: Name (TEAMID)"]
set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR=build
OUT=dist
VERSION=0.1.0
SIGN_ID=""
IR_LIB="$HOME/Library/Audio/Impulse Responses/DataCogs"
SIGN_AAX=0
PACE_ACCOUNT="${PACE_ACCOUNT:-kogzee}"
PACE_WCGUID="${PACE_WCGUID:-FCB93630-951E-11F1-9B0E-00505692AD3E}"
AAX_SIGNID="${AAX_SIGNID:-Developer ID Application: Mark Daunt (74N92K2DPP)}"
WRAPTOOL="${WRAPTOOL:-/Applications/PACEAntiPiracy/Eden/Fusion/Versions/6/bin/wraptool}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)    BUILD_DIR="$2"; shift 2 ;;
    --ir-library)   IR_LIB="$2"; shift 2 ;;
    --out)          OUT="$2"; shift 2 ;;
    --version)      VERSION="$2"; shift 2 ;;
    --sign)         SIGN_ID="$2"; shift 2 ;;
    --sign-aax)     SIGN_AAX=1; shift ;;
    --pace-account) PACE_ACCOUNT="$2"; shift 2 ;;
    --pace-wcguid)  PACE_WCGUID="$2"; shift 2 ;;
    --aax-signid)   AAX_SIGNID="$2"; shift 2 ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done

STAGE=$(mktemp -d)
CLOUD_SESSION=0
cleanup() {
  if [[ $CLOUD_SESSION -eq 1 ]]; then
    iloktool cloud --close > /dev/null 2>&1 || true
  fi
  rm -rf "$STAGE"
}
trap cleanup EXIT
mkdir -p "$OUT"

# ---- component 1: the plugins ----------------------------------------------
PLUG_ROOT="$STAGE/plugins-root"
AU_DIR="$PLUG_ROOT/Library/Audio/Plug-Ins/Components"
VST3_DIR="$PLUG_ROOT/Library/Audio/Plug-Ins/VST3"
AAX_DIR="$PLUG_ROOT/Library/Application Support/Avid/Audio/Plug-Ins"
mkdir -p "$AU_DIR" "$VST3_DIR" "$AAX_DIR"

# Newest bundle of the given extension in a dir. Renames leave stale
# old-name bundles behind in local build trees; CI trees only ever hold one.
newest_bundle() {
  local found
  found=$(ls -td "$1"/*."$2" 2>/dev/null | head -1)
  [[ -n "$found" ]] || { echo "error: no .$2 bundle in $1" >&2; exit 1; }
  if [[ $(ls -d "$1"/*."$2" | wc -l) -gt 1 ]]; then
    echo "warning: multiple .$2 bundles in $1 - packaging newest: $(basename "$found")" >&2
  fi
  echo "$found"
}

for entry in compressor:CompressorPlugin parametric-eq:ParametricEQPlugin convolution-reverb:ConvolutionReverbPlugin; do
  dir="${entry%%:*}"; target="${entry##*:}"
  art="$BUILD_DIR/plugins/$dir/plugin/${target}_artefacts"
  [[ -d "$art/Release" ]] && art="$art/Release"   # multi-config generators

  cp -R "$(newest_bundle "$art/AU" component)" "$AU_DIR/"
  cp -R "$(newest_bundle "$art/VST3" vst3)" "$VST3_DIR/"

  aax=$(newest_bundle "$art/AAX" aaxplugin)
  signed="/Library/Application Support/Avid/Audio/Plug-Ins/$(basename "$aax")"
  if [[ $SIGN_AAX -eq 0 && -d "$signed" ]]; then
    cp -R "$signed" "$AAX_DIR/"     # already wraptool-signed by the dev build
  else
    [[ $SIGN_AAX -eq 1 ]] || echo "warning: no signed AAX for $(basename "$aax") - packaging the unsigned build-tree copy" >&2
    cp -R "$aax" "$AAX_DIR/"
  fi
done

# Sign the staged AAX bundles in place. With a USB iLok (or an open iLok
# Cloud session) no password is needed; CI exports PACE_PASSWORD to open a
# cloud session headlessly.
if [[ $SIGN_AAX -eq 1 ]]; then
  [[ -x "$WRAPTOOL" ]] || { echo "error: wraptool not found at $WRAPTOOL" >&2; exit 1; }
  CLOUD_ARGS=()
  if [[ -n "${PACE_PASSWORD:-}" ]]; then
    # Headless mode: open the iLok Cloud session (iloktool ships with the
    # iLok License Support installer and lands on PATH on macOS), then let
    # wraptool use PACE's cloud signing service. Closed again by cleanup().
    iloktool cloud --open --account "$PACE_ACCOUNT" --password "$PACE_PASSWORD" -v
    CLOUD_SESSION=1
    CLOUD_ARGS=(--allowsigningservice)
  fi
  for aax in "$AAX_DIR"/*.aaxplugin; do
    echo "wraptool signing: $(basename "$aax")"
    "$WRAPTOOL" sign --account "$PACE_ACCOUNT" \
                --wcguid "$PACE_WCGUID" --signid "$AAX_SIGNID" \
                --in "$aax" --out "$aax" \
                ${CLOUD_ARGS[@]+"${CLOUD_ARGS[@]}"}
  done
fi

pkgbuild --root "$PLUG_ROOT" \
         --identifier com.datacogs.plugins.suite \
         --version "$VERSION" \
         --install-location / \
         "$STAGE/DataCogsPlugins.pkg" > /dev/null

# ---- component 2: the IR library -------------------------------------------
IR_ROOT="$STAGE/ir-root/Library/Audio/Impulse Responses/DataCogs"
mkdir -p "$IR_ROOT"
# favourites.txt is per-user state, never shipped
rsync -a --exclude favourites.txt "$IR_LIB/" "$IR_ROOT/"

pkgbuild --root "$STAGE/ir-root" \
         --identifier com.datacogs.plugins.irlibrary \
         --version "$VERSION" \
         --install-location / \
         "$STAGE/DataCogsIRLibrary.pkg" > /dev/null

# ---- distribution ----------------------------------------------------------
cat > "$STAGE/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>DataCogs Plugins $VERSION</title>
    <options customize="allow" require-scripts="false"/>
    <choices-outline>
        <line choice="plugins"/>
        <line choice="irlibrary"/>
    </choices-outline>
    <choice id="plugins" title="DataCogs Plugins"
            description="Compressor, EQ and Reverb (AU, VST3, AAX).">
        <pkg-ref id="com.datacogs.plugins.suite"/>
    </choice>
    <choice id="irlibrary" title="Impulse Response Library"
            description="Impulse responses and artwork for DataCogs Reverb.">
        <pkg-ref id="com.datacogs.plugins.irlibrary"/>
    </choice>
    <pkg-ref id="com.datacogs.plugins.suite">DataCogsPlugins.pkg</pkg-ref>
    <pkg-ref id="com.datacogs.plugins.irlibrary">DataCogsIRLibrary.pkg</pkg-ref>
</installer-gui-script>
XML

FINAL="$OUT/DataCogs Plugins-$VERSION.pkg"
productbuild --distribution "$STAGE/distribution.xml" \
             --package-path "$STAGE" \
             ${SIGN_ID:+--sign "$SIGN_ID"} \
             "$FINAL"

echo "Built: $FINAL ($(du -h "$FINAL" | cut -f1 | tr -d ' '))"
[[ -z "$SIGN_ID" ]] && echo "note: unsigned - pass --sign for a distributable installer (then notarize)."
