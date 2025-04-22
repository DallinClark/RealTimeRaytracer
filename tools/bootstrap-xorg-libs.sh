#!/usr/bin/env bash
set -euo pipefail

# 1) Configuration
PKGS=(libXi-devel libXcursor-devel libXinerama-devel libXrandr-devel)
TMPDIR="$HOME/tmp/rpms"
PREFIX="$HOME/.local"

# 2) Prepare directories
echo "→ Creating $TMPDIR and $PREFIX …"
mkdir -p "$TMPDIR" "$PREFIX"

# 3) Download RPMs
echo "→ Downloading RPMs for: ${PKGS[*]} …"
cd "$TMPDIR"
dnf download --resolve "${PKGS[@]}"

# 4) Unpack into $PREFIX
echo "→ Unpacking into $PREFIX …"
for rpm in ./*.rpm; do
  echo "   • $rpm"
  rpm2cpio "$rpm" | (cd "$PREFIX" && cpio -idmv)
done

# 5) Optional: clean up downloaded RPMs
# rm -rf "$TMPDIR"/*.rpm

# 6) Ensure env vars in ~/.bashrc
declare -a LINES=(
  'export PKG_CONFIG_PATH="$HOME/.local/usr/lib64/pkgconfig:$PKG_CONFIG_PATH"'
  'export LD_LIBRARY_PATH="$HOME/.local/usr/lib64:$LD_LIBRARY_PATH"'
  'export CMAKE_PREFIX_PATH="$HOME/.local/usr:$CMAKE_PREFIX_PATH"'
)

echo "→ Adding environment exports to ~/.bashrc if missing…"
for line in "${LINES[@]}"; do
  if ! grep -Fxq "$line" "$HOME/.bashrc"; then
    echo "$line" >> "$HOME/.bashrc"
    echo "   ✓ Added: $line"
  else
    echo "   • Already present: $line"
  fi
done

echo
echo "✅ Done! Open a new terminal or run 'source ~/.bashrc' to pick up the changes."
