#!/bin/bash
# Builds a macOS installer .pkg for Harmonic Compressor (VST3, AU, CLAP, LV2, AAX). No Standalone app ships.
# Usage: ./Scripts/build_installer.sh [folder with the built bundles]
#
# The folder is the --out of OpenPluginSDK's scripts/build_plugin.sh (default plugin/target/dist-<version>):
# the signed bundles side by side, the AAX already PACE-signed.

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRODUCT_NAME="Harmonic Compressor"
VERSION="$(tr -d '\r\n' < "$PROJECT_ROOT/VERSION")"
ARTEFACTS="${1:-$PROJECT_ROOT/plugin/target/dist-$VERSION}"
grep -q "^version = \"$VERSION\"" "$PROJECT_ROOT/plugin/Cargo.toml" || { echo "ERROR: VERSION ($VERSION) is not the crate's version" >&2; exit 1; }
IDENTIFIER="com.opengrid.harmoniccompressor"
OUT_DIR="${HC_INSTALLER_OUT_DIR:-$PROJECT_ROOT/Installer}"
STAGING="$OUT_DIR/staging"

INSTALLER_ID="Developer ID Installer: Marcos Deida (ZQYU67JUBH)"
NOTARY_PROFILE="AC_NOTARY"

VST3_SRC="$ARTEFACTS/$PRODUCT_NAME.vst3"
AU_SRC="$ARTEFACTS/$PRODUCT_NAME.component"
CLAP_SRC="$ARTEFACTS/$PRODUCT_NAME.clap"
LV2_SRC="$ARTEFACTS/${PRODUCT_NAME// /}.lv2"   # installed under 1.5.x's folder name, "Harmonic Compressor.lv2"
AAX_SRC="$ARTEFACTS/$PRODUCT_NAME.aaxplugin"
MANUAL_PDF="$PROJECT_ROOT/output/pdf/HarmonicCompressor-User-Manual.pdf"
LICENSE_BINARIES_SRC="$PROJECT_ROOT/LICENSE-BINARIES.txt"
LICENSE_MIT_SRC="$PROJECT_ROOT/LICENSE"

echo "== Harmonic Compressor installer builder =="
echo "Artefacts dir: $ARTEFACTS"

COMPONENT_PKG="$OUT_DIR/${PRODUCT_NAME// /}-component.pkg"
UNSIGNED_PKG="$OUT_DIR/${PRODUCT_NAME// /}-$VERSION-macOS-unsigned.pkg"
FINAL_PKG="$OUT_DIR/${PRODUCT_NAME// /-}-$VERSION-macos.pkg"   # the published name
COMPONENT_PLIST="$OUT_DIR/component.plist"
RESOURCES_DIR="$OUT_DIR/resources"
DISTRIBUTION_XML="$OUT_DIR/distribution.xml"

# Remove only what this script writes: OUT_DIR may be a folder that holds other files.
SCRIPTS_DIR="$OUT_DIR/scripts"
rm -rf "$STAGING" "$RESOURCES_DIR" "$SCRIPTS_DIR"
rm -f "$COMPONENT_PKG" "$UNSIGNED_PKG" "$FINAL_PKG" "$FINAL_PKG.installs.json" "$COMPONENT_PLIST" "$DISTRIBUTION_XML"
mkdir -p "$STAGING/Library/Audio/Plug-Ins/VST3"
mkdir -p "$STAGING/Library/Audio/Plug-Ins/Components"
mkdir -p "$STAGING/Library/Audio/Plug-Ins/CLAP"
mkdir -p "$STAGING/Library/Audio/Plug-Ins/LV2"
mkdir -p "$STAGING/Library/Application Support/Avid/Audio/Plug-Ins"

DOCS_DEST="$STAGING/Library/Application Support/Harmonic Compressor"
mkdir -p "$DOCS_DEST"
if [ -d "$PROJECT_ROOT/Licenses" ]; then
    ditto "$PROJECT_ROOT/Licenses" "$DOCS_DEST/Licenses"
    # licences of the 1.5.x source's components, which are not in these binaries
    rm -f "$DOCS_DEST/Licenses/third-party/JUCE-LICENSE.md" "$DOCS_DEST/Licenses/third-party/GoodLookinUI-LICENSE.txt" "$DOCS_DEST/Licenses/third-party/clap-juce-extensions-LICENSE.md"
fi
if [ -f "$PROJECT_ROOT/THIRD_PARTY_LICENSES.md" ]; then
    cp "$PROJECT_ROOT/THIRD_PARTY_LICENSES.md" "$DOCS_DEST/THIRD_PARTY_LICENSES.md"
fi
if [ -f "$LICENSE_BINARIES_SRC" ]; then
    cp "$LICENSE_BINARIES_SRC" "$DOCS_DEST/ReadMe - License (Freeware).txt"
    echo "  + LICENSE-BINARIES.txt included"
fi
if [ -f "$LICENSE_MIT_SRC" ]; then
    cp "$LICENSE_MIT_SRC" "$DOCS_DEST/LICENSE (MIT, source code).txt"
fi
if [ -f "$MANUAL_PDF" ]; then
    cp "$MANUAL_PDF" "$DOCS_DEST/Harmonic Compressor User Manual.pdf"
    echo "  + User Manual PDF included"
fi

FOUND_ANY=0
INSTALLS=()   # format|install path, for the Plugin Manager sidecar

if [ -d "$VST3_SRC" ]; then
    ditto "$VST3_SRC" "$STAGING/Library/Audio/Plug-Ins/VST3/$PRODUCT_NAME.vst3"
    INSTALLS+=("vst3|Library/Audio/Plug-Ins/VST3/$PRODUCT_NAME.vst3")
    echo "  + VST3 included"
    FOUND_ANY=1
else
    rmdir "$STAGING/Library/Audio/Plug-Ins/VST3" 2>/dev/null || true
fi

if [ -d "$AU_SRC" ]; then
    ditto "$AU_SRC" "$STAGING/Library/Audio/Plug-Ins/Components/$PRODUCT_NAME.component"
    INSTALLS+=("au|Library/Audio/Plug-Ins/Components/$PRODUCT_NAME.component")
    echo "  + AU included"
    FOUND_ANY=1
else
    rmdir "$STAGING/Library/Audio/Plug-Ins/Components" 2>/dev/null || true
fi

if [ -d "$CLAP_SRC" ]; then
    ditto "$CLAP_SRC" "$STAGING/Library/Audio/Plug-Ins/CLAP/$PRODUCT_NAME.clap"
    INSTALLS+=("clap|Library/Audio/Plug-Ins/CLAP/$PRODUCT_NAME.clap")
    echo "  + CLAP included"
    FOUND_ANY=1
else
    rmdir "$STAGING/Library/Audio/Plug-Ins/CLAP" 2>/dev/null || true
fi

if [ -d "$LV2_SRC" ]; then
    ditto "$LV2_SRC" "$STAGING/Library/Audio/Plug-Ins/LV2/$PRODUCT_NAME.lv2"
    INSTALLS+=("lv2|Library/Audio/Plug-Ins/LV2/$PRODUCT_NAME.lv2")
    echo "  + LV2 included"
    FOUND_ANY=1
else
    rmdir "$STAGING/Library/Audio/Plug-Ins/LV2" 2>/dev/null || true
fi

if [ -d "$AAX_SRC" ]; then
    ditto "$AAX_SRC" "$STAGING/Library/Application Support/Avid/Audio/Plug-Ins/$PRODUCT_NAME.aaxplugin"
    INSTALLS+=("aax|Library/Application Support/Avid/Audio/Plug-Ins/$PRODUCT_NAME.aaxplugin")
    echo "  + AAX included (PACE Eden signed)"
    FOUND_ANY=1
else
    rmdir -p "$STAGING/Library/Application Support/Avid/Audio/Plug-Ins" 2>/dev/null || true   # and the empty Avid folders
fi

if [ "$FOUND_ANY" -eq 0 ]; then
    echo "ERROR: No built artefacts found under $ARTEFACTS. Build the plugin first." >&2
    exit 1
fi

pkgbuild --analyze --root "$STAGING" "$COMPONENT_PLIST"
/usr/bin/python3 - "$COMPONENT_PLIST" <<'PYEOF'
import plistlib, sys
path = sys.argv[1]
with open(path, "rb") as f:
    data = plistlib.load(f)
for entry in data:
    entry["BundleIsVersionChecked"] = False
    entry["BundleIsRelocatable"] = False
    # Replacing bundles removes obsolete resources from earlier installations;
    # merging leaves extra files that invalidate the new code signature.
    entry["BundleOverwriteAction"] = "upgrade"
with open(path, "wb") as f:
    plistlib.dump(data, f)
PYEOF

# An LV2 folder is not a bundle to the installer, so an upgrade would merge into it: remove 1.5.x's first.
mkdir -p "$SCRIPTS_DIR"
printf '#!/bin/sh\nrm -rf "/Library/Audio/Plug-Ins/LV2/%s.lv2"\nexit 0\n' "$PRODUCT_NAME" > "$SCRIPTS_DIR/preinstall"
chmod 755 "$SCRIPTS_DIR/preinstall"

pkgbuild \
    --root "$STAGING" \
    --scripts "$SCRIPTS_DIR" \
    --component-plist "$COMPONENT_PLIST" \
    --identifier "$IDENTIFIER" \
    --version "$VERSION" \
    --install-location "/" \
    "$COMPONENT_PKG"

mkdir -p "$RESOURCES_DIR"

if [ -f "$LICENSE_BINARIES_SRC" ]; then
    cp "$LICENSE_BINARIES_SRC" "$RESOURCES_DIR/License.txt"
    cp "$LICENSE_BINARIES_SRC" "$RESOURCES_DIR/ReadMe.txt"
fi

productbuild --synthesize --package "$COMPONENT_PKG" "$DISTRIBUTION_XML"

if [ -f "$RESOURCES_DIR/License.txt" ]; then
    /usr/bin/python3 - "$DISTRIBUTION_XML" <<'PYEOF'
import sys
path = sys.argv[1]
with open(path, "r") as f:
    xml = f.read()
insert = '    <license file="License.txt" mime-type="text/plain"/>\n    <readme file="ReadMe.txt" mime-type="text/plain"/>\n'
marker = "<installer-gui-script"
idx = xml.find(marker)
if idx == -1:
    raise SystemExit("could not find <installer-gui-script> root element")
end_of_open_tag = xml.find(">", idx) + 1
xml = xml[:end_of_open_tag] + "\n" + insert + xml[end_of_open_tag:]
with open(path, "w") as f:
    f.write(xml)
PYEOF
fi

echo "== Synthesizing product distribution package =="
productbuild \
    --distribution "$DISTRIBUTION_XML" \
    --package-path "$OUT_DIR" \
    --resources "$RESOURCES_DIR" \
    "$UNSIGNED_PKG"

echo "== Signing installer package with Developer ID Installer =="
productsign --sign "$INSTALLER_ID" --timestamp "$UNSIGNED_PKG" "$FINAL_PKG"
rm -f "$UNSIGNED_PKG"

echo "== Submitting installer package to Apple Notary Service =="
xcrun notarytool submit "$FINAL_PKG" --keychain-profile "$NOTARY_PROFILE" --wait

echo "== Stapling notarization ticket to installer package =="
for retry in 1 2 3; do
    if xcrun stapler staple "$FINAL_PKG"; then
        break
    fi
    sleep 3
done
xcrun stapler validate "$FINAL_PKG"

echo "== Gatekeeper assessment =="
spctl --assess --type install -v "$FINAL_PKG"

# What the package installs, for tools/write_opengrid_manifest.py. The architecture is read from the binary.
FIRST_BINARY="$(find "$STAGING/Library" -type f -perm +111 -path '*/Contents/MacOS/*' | head -1)"
case "$(lipo -archs "$FIRST_BINARY")" in
    *arm64*x86_64*|*x86_64*arm64*) ARCH=universal ;;
    *) ARCH="$(lipo -archs "$FIRST_BINARY")" ;;
esac
{
    printf '{"os":"macos","arch":"%s","build_class":"modern","min_os":"11.0","installs":[' "$ARCH"
    sep=""
    for entry in "${INSTALLS[@]}"; do
        printf '%s{"format":"%s","path":"%s"}' "$sep" "${entry%%|*}" "${entry#*|}"
        sep=","
    done
    printf ']}\n'
} > "$FINAL_PKG.installs.json"

rm -rf "$STAGING" "$COMPONENT_PKG" "$COMPONENT_PLIST" "$DISTRIBUTION_XML" "$RESOURCES_DIR" "$SCRIPTS_DIR"

echo ""
echo "== Done =="
echo "Notarized installer package: $FINAL_PKG (installs list: $FINAL_PKG.installs.json)"
