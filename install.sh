#!/usr/bin/env zsh
# install.sh — symlink cc into PATH

set -euo pipefail

SCRIPT_DIR="${0:A:h}"
TARGET="$HOME/.local/bin/cc"

mkdir -p "$HOME/.local/bin"

if [[ -e "$TARGET" && ! -L "$TARGET" ]]; then
    echo "  $TARGET exists and is not a symlink. Backing up to ${TARGET}.bak"
    mv "$TARGET" "${TARGET}.bak"
fi

ln -sf "$SCRIPT_DIR/cc" "$TARGET"
chmod +x "$SCRIPT_DIR/cc"

echo "  Installed: cc → $TARGET"
echo ""
echo "  Make sure ~/.local/bin is in your PATH:"
echo "    export PATH=\"\$HOME/.local/bin:\$PATH\""
echo ""
echo "  Then run: cc"
