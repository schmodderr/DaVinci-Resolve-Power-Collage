#!/bin/sh
# Installs the prebuilt Power Collage OpenFX plugin for DaVinci Resolve (macOS).
# Double-click in Finder or run in Terminal. Asks for the admin password,
# because OpenFX plugins live in /Library/OFX/Plugins.
set -e

REPO="$(cd "$(dirname "$0")" && pwd)"
BUNDLE="$REPO/Plugin/PowerCollage.ofx.bundle"
TARGET="/Library/OFX/Plugins"

if [ ! -d "$BUNDLE" ]; then
	echo "Plugin not found: $BUNDLE"
	exit 1
fi

echo "Installing Power Collage into $TARGET (admin password required)"
sudo mkdir -p "$TARGET"
sudo rm -rf "$TARGET/PowerCollage.ofx.bundle"
sudo cp -R "$BUNDLE" "$TARGET/"
# Files downloaded from the internet are quarantined and macOS would refuse to load the plugin.
sudo xattr -dr com.apple.quarantine "$TARGET/PowerCollage.ofx.bundle" 2>/dev/null || true
echo "Done. Restart DaVinci Resolve, the effect is under Effects > Open FX > Power Collage."
