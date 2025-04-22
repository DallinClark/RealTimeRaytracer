#!/usr/bin/env bash
set -euo pipefail

# ─── Configuration ─────────────────────────────────────────────────────────────

# LLVM version to fetch
LLVM_VER="20.1.0"
# “Platform” string as in the GitHub release asset name
PLATFORM="Linux-X64"
# Base URL for the GitHub release
URL_BASE="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VER}"

# Destination directory under your home
DEST="$HOME/tools/llvm"
# Name of the tarball to download
TARBALL="LLVM-${LLVM_VER}-${PLATFORM}.tar.xz"
# Expected unpacked directory name
UNPACK_DIR="${DEST}/LLVM-${LLVM_VER}-${PLATFORM}"

# ─── Bootstrap ────────────────────────────────────────────────────────────────

mkdir -p "${DEST}"

if [[ -d "${UNPACK_DIR}" ]]; then
  echo "✔ LLVM ${LLVM_VER} already unpacked in ${UNPACK_DIR}"
  exit 0
fi

cd "${DEST}"

echo "⇣ Downloading LLVM ${LLVM_VER} for ${PLATFORM}..."
curl -L -o "${TARBALL}" "${URL_BASE}/${TARBALL}"

echo "⇳ Extracting ${TARBALL}..."
tar -xf "${TARBALL}"

echo "🗑 Removing archive ${TARBALL}..."
rm "${TARBALL}"

echo "✔ LLVM ${LLVM_VER} is ready in ${UNPACK_DIR}"
