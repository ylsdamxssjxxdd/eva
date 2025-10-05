#!/usr/bin/env bash
set -euo pipefail

# package-appimage.sh - produce an AppImage from an existing build
# Usage: tools/package-appimage.sh [build-dir] [AppDir] [config]
#  - build-dir defaults to ./build
#  - AppDir defaults to <build-dir>/AppDir
#  - config (optional): Release/Debug/etc.; if empty, omitted

# Make AppImage tools work without FUSE (fallback)
export APPIMAGE_EXTRACT_AND_RUN=1

BUILD_DIR=${1:-build}
APPDIR=${2:-"${BUILD_DIR}/AppDir"}
CONFIG=${3:-}

# Ensure build exists
if [[ ! -d "$BUILD_DIR" ]]; then
  echo "Build directory '$BUILD_DIR' not found. Run cmake first." >&2
  exit 1
fi

mkdir -p "$APPDIR"

# Discover or fetch linuxdeploy, plugin-qt, appimagetool
CACHE_DIR="$(pwd)/tools/.cache"
mkdir -p "$CACHE_DIR"

need_fetch() { [[ ! -x "$1" ]]; }

fetch_if_missing() {
  local url="$1" dst="$2"
  echo "Downloading $(basename "$dst") ..."
  curl -L --fail "$url" -o "$dst"
  chmod +x "$dst"
}

LINUXDEPLOY="${LINUXDEPLOY:-$(command -v linuxdeploy || true)}"
PLUGIN_QT="${PLUGIN_QT:-$(command -v linuxdeploy-plugin-qt || true)}"
APPIMAGETOOL="${APPIMAGETOOL:-$(command -v appimagetool || true)}"

if [[ -z "$LINUXDEPLOY" ]]; then
  LINUXDEPLOY="$CACHE_DIR/linuxdeploy"
fi
if [[ -z "$PLUGIN_QT" ]]; then
  PLUGIN_QT="$CACHE_DIR/linuxdeploy-plugin-qt"
fi
if [[ -z "$APPIMAGETOOL" ]]; then
  APPIMAGETOOL="$CACHE_DIR/appimagetool"
fi

if need_fetch "$LINUXDEPLOY"; then
  fetch_if_missing "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" "$LINUXDEPLOY"
fi
if need_fetch "$PLUGIN_QT"; then
  fetch_if_missing "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" "$PLUGIN_QT"
fi
if need_fetch "$APPIMAGETOOL"; then
  fetch_if_missing "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage" "$APPIMAGETOOL"
fi

# Ensure AppDir/usr tree via cmake install
if [[ -n "$CONFIG" ]]; then
  cmake --install "$BUILD_DIR" --config "$CONFIG" --prefix "$APPDIR/usr"
else
  cmake --install "$BUILD_DIR" --prefix "$APPDIR/usr"
fi

# The main executable and desktop/icon locations after install()
MAIN_EXE="$APPDIR/usr/bin/eva"
DESKTOP_FILE="$APPDIR/usr/share/applications/eva.desktop"
ICON_FILE="$APPDIR/usr/share/icons/hicolor/64x64/apps/blue_logo.png"

# Sanity checks
for f in "$MAIN_EXE" "$DESKTOP_FILE" "$ICON_FILE"; do
  [[ -e "$f" ]] || { echo "Missing $f; did the build succeed?" >&2; exit 2; }
fi

# Run linuxdeploy with Qt plugin
export QMAKE="${QMAKE:-$(command -v qmake || true)}"

"$LINUXDEPLOY" --appdir "$APPDIR" \
  -e "$MAIN_EXE" \
  -d "$DESKTOP_FILE" \
  -i "$ICON_FILE"

# Run the Qt plugin to bundle Qt runtime
"$PLUGIN_QT" --appdir "$APPDIR"

# Build the AppImage
"$APPIMAGETOOL" "$APPDIR"

# Output location note
ls -lh ./*.AppImage "$BUILD_DIR"/*.AppImage 2>/dev/null || true