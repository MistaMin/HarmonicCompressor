#!/bin/bash
# Builds a macOS installer .pkg for Harmonic Compressor (VST3, AU, AAX, Standalone app).
# Usage: ./Scripts/build_installer.sh [path-to-build-dir]
#
# Requires that the plugin has already been built in Release configuration
# (VST3, AU, AAX, Standalone targets) via CMake/Xcode.

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$PROJECT_ROOT/build}"
ARTEFACTS="$BUILD_DIR/HarmonicCompressor_artefacts/Release"

PRODUCT_NAME="Harmonic Compressor"
VERSION="1.3.0"
IDENTIFIER="com.mincomp.harmoniccompressor"
OUT_DIR="$PROJECT_ROOT/Installer"
STAGING="$OUT_DIR/staging"

VST3_SRC="$ARTEFACTS/VST3/$PRODUCT_NAME.vst3"
AU_SRC="$ARTEFACTS/AU/$PRODUCT_NAME.component"
AAX_SRC="$ARTEFACTS/AAX/$PRODUCT_NAME.aaxplugin"
APP_SRC="$ARTEFACTS/Standalone/$PRODUCT_NAME.app"

echo "== Harmonic Compressor installer builder =="
echo "Artefacts dir: $ARTEFACTS"

rm -rf "$OUT_DIR"
mkdir -p "$STAGING/Library/Audio/Plug-Ins/VST3"
mkdir -p "$STAGING/Library/Audio/Plug-Ins/Components"
mkdir -p "$STAGING/Library/Application Support/Avid/Audio/Plug-Ins"
mkdir -p "$STAGING/Applications"

FOUND_ANY=0

if [ -d "$VST3_SRC" ]; then
    cp -R "$VST3_SRC" "$STAGING/Library/Audio/Plug-Ins/VST3/"
    echo "  + VST3 included"
    FOUND_ANY=1
else
    echo "  - VST3 artefact not found, skipping ($VST3_SRC)"
    rmdir "$STAGING/Library/Audio/Plug-Ins/VST3" 2>/dev/null || true
fi

if [ -d "$AU_SRC" ]; then
    cp -R "$AU_SRC" "$STAGING/Library/Audio/Plug-Ins/Components/"
    echo "  + AU included"
    FOUND_ANY=1
else
    echo "  - AU artefact not found, skipping ($AU_SRC)"
    rmdir "$STAGING/Library/Audio/Plug-Ins/Components" 2>/dev/null || true
fi

if [ -d "$AAX_SRC" ]; then
    cp -R "$AAX_SRC" "$STAGING/Library/Application Support/Avid/Audio/Plug-Ins/"
    echo "  + AAX included (NOTE: unsigned/unauthorized - see README)"
    FOUND_ANY=1
else
    echo "  - AAX artefact not found, skipping ($AAX_SRC)"
    rmdir "$STAGING/Library/Application Support/Avid/Audio/Plug-Ins" 2>/dev/null || true
fi

if [ -d "$APP_SRC" ]; then
    cp -R "$APP_SRC" "$STAGING/Applications/"
    echo "  + Standalone app included"
    FOUND_ANY=1
else
    echo "  - Standalone artefact not found, skipping ($APP_SRC)"
    rmdir "$STAGING/Applications" 2>/dev/null || true
fi

if [ "$FOUND_ANY" -eq 0 ]; then
    echo "ERROR: No built artefacts found under $ARTEFACTS. Build the plugin first." >&2
    exit 1
fi

COMPONENT_PKG="$OUT_DIR/${PRODUCT_NAME// /}-component.pkg"
FINAL_PKG="$OUT_DIR/${PRODUCT_NAME// /}-$VERSION-macOS.pkg"
COMPONENT_PLIST="$OUT_DIR/component.plist"

# All four build products (VST3/AU/AAX/Standalone) share the same
# CFBundleIdentifier (JUCE gives every plugin wrapper of one product the
# same bundle id). pkgbuild's default auto-generated component plist marks
# bundles "BundleIsVersionChecked", which makes the installer treat the 2nd,
# 3rd, 4th bundle with the same id+version as "already installed" once the
# first one lands during the same install pass, and it silently skips
# copying the rest. Generate an explicit component plist and turn that
# check (and relocation) off for every bundle so each one is always copied.
pkgbuild --analyze --root "$STAGING" "$COMPONENT_PLIST"
/usr/bin/python3 - "$COMPONENT_PLIST" <<'PYEOF'
import plistlib, sys
path = sys.argv[1]
with open(path, "rb") as f:
    data = plistlib.load(f)
for entry in data:
    entry["BundleIsVersionChecked"] = False
    entry["BundleIsRelocatable"] = False
with open(path, "wb") as f:
    plistlib.dump(data, f)
PYEOF

pkgbuild \
    --root "$STAGING" \
    --component-plist "$COMPONENT_PLIST" \
    --identifier "$IDENTIFIER" \
    --version "$VERSION" \
    --install-location "/" \
    "$COMPONENT_PKG"

productbuild \
    --package "$COMPONENT_PKG" \
    "$FINAL_PKG"

rm -rf "$STAGING" "$COMPONENT_PKG" "$COMPONENT_PLIST"

echo ""
echo "== Done =="
echo "Installer package: $FINAL_PKG"
echo "Install with:  sudo installer -pkg \"$FINAL_PKG\" -target /"
echo "(Package is unsigned - Gatekeeper will require right-click > Open, or the sudo installer command above.)"
