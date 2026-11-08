#!/bin/bash
# Stages the plugin bundles in the folders the plugin manager installs them to, and zips them.
# The zip root mirrors the macOS install tree, so the manager copies each entry to "/" as-is.
#
# Usage: make_bundle_zip.sh <Plugin Name> <version> <artefacts/Release dir> <out dir>
# Set INCLUDE_STANDALONE=1 only when the user requests it for the Manager ZIP.
set -euo pipefail

if [ $# -ne 4 ]; then
  echo "usage: $0 <Plugin Name> <version> <artefacts/Release dir> <out dir>" >&2
  exit 2
fi

PLUGIN=$1
VERSION=$2
ART=$3
OUT=$4

mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
STAGE="$OUT/stage-macos-bundles"
NAME="${PLUGIN// /-}-$VERSION-macos.zip"
ZIP="$OUT/$NAME"

rm -rf "$STAGE"
mkdir -p "$STAGE"

# Format folder under artefacts | bundle name | install folder (relative to /) | manifest format name
declare -a BUNDLES=(
  "VST3|%s.vst3|Library/Audio/Plug-Ins/VST3|vst3"
  "AU|%s.component|Library/Audio/Plug-Ins/Components|au"
  "CLAP|%s.clap|Library/Audio/Plug-Ins/CLAP|clap"
  "LV2|%s.lv2|Library/Audio/Plug-Ins/LV2|lv2"
  "AAX|%s.aaxplugin|Library/Application Support/Avid/Audio/Plug-Ins|aax"
)
case "${INCLUDE_STANDALONE:-0}" in
  1) BUNDLES+=("Standalone|%s.app|Applications|app") ;;
  0) ;;
  *) echo "INCLUDE_STANDALONE must be 0 or 1" >&2; exit 2 ;;
esac

INSTALLS=()
for entry in "${BUNDLES[@]}"; do
  IFS='|' read -r fmt pattern dest id <<<"$entry"
  name=$(printf "$pattern" "$PLUGIN")
  src="$ART/$fmt/$name"
  if [ ! -e "$src" ]; then
    echo "missing $src" >&2
    exit 1
  fi
  mkdir -p "$STAGE/$dest"
  ditto --norsrc --noextattr --noacl "$src" "$STAGE/$dest/$name"
  xattr -cr "$STAGE/$dest/$name" 2>/dev/null || true
  INSTALLS+=("$id|$dest/$name")
done

# Optional documentation & licenses under Library/Application Support/<Plugin>
if [ -n "${RESOURCES_DIR:-}" ] && [ -d "$RESOURCES_DIR" ]; then
  DOCS_DEST="$STAGE/Library/Application Support/$PLUGIN"
  mkdir -p "$DOCS_DEST"
  ditto --norsrc --noextattr --noacl "$RESOURCES_DIR" "$DOCS_DEST"
  xattr -cr "$DOCS_DEST" 2>/dev/null || true
fi

if find "$STAGE" -name '._*' | grep . >/dev/null; then
  echo "AppleDouble files in the stage; refusing to zip" >&2
  exit 1
fi

rm -f "$ZIP"
(cd "$STAGE" && COPYFILE_DISABLE=1 zip -qry "$ZIP" .)
rm -rf "$STAGE"

# Installs list, one entry per bundle, for the manifest writer.
{
  printf '{"os":"macos","arch":"universal","build_class":"modern","min_os":"%s","installs":[' "${MIN_OS:-11.0}"
  first=1
  for entry in "${INSTALLS[@]}"; do
    id=${entry%%|*}; rel=${entry#*|}
    [ $first -eq 1 ] || printf ','
    printf '{"format":"%s","path":"%s"}' "$id" "$rel"
    first=0
  done
  printf ']}\n'
} > "$ZIP.installs.json"

echo "Wrote $ZIP and $ZIP.installs.json"
