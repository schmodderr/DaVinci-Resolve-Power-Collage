#!/bin/sh
# Installs Power Collage for the current user (macOS, Windows via Git Bash, Linux).
# Usage: ./install.sh          copy the files
#        ./install.sh --link   symlink them, so edits in this repository apply directly
set -e

case "$(uname -s)" in
	Darwin) FUSION="$HOME/Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion" ;;
	MINGW*|MSYS*|CYGWIN*) FUSION="$APPDATA/Blackmagic Design/DaVinci Resolve/Support/Fusion" ;;
	*) FUSION="$HOME/.local/share/DaVinciResolve/Fusion" ;;
esac

REPO="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$FUSION/Fuses" "$FUSION/Templates/Edit/Effects"

install_file() {
	if [ "$1" = "--link" ]; then
		ln -sf "$2" "$3"
	else
		cp "$2" "$3"
	fi
	echo "  $3"
}

echo "Installing Power Collage into $FUSION"
install_file "$1" "$REPO/Fuses/PowerCollage.fuse" "$FUSION/Fuses/PowerCollage.fuse"
install_file "$1" "$REPO/Templates/Edit/Effects/Power Collage.setting" "$FUSION/Templates/Edit/Effects/Power Collage.setting"
echo "Done. Restart DaVinci Resolve to load the Fuse."
