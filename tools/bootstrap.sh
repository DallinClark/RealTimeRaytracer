#!/usr/bin/env bash
set -euo pipefail

# ────────────────────────────────────────────────────────────────────────────────
# 1) System‑level X11 dev libraries via RPM → $HOME/.local
# ────────────────────────────────────────────────────────────────────────────────
PKGS=(libXi-devel libXcursor-devel libXinerama-devel libXrandr-devel)
RPM_TMP="$HOME/tmp/rpms"
LOCAL_PREFIX="$HOME/.local"

echo "→ Installing system X11 dev libs into $LOCAL_PREFIX …"
mkdir -p "$RPM_TMP" "$LOCAL_PREFIX"
cd "$RPM_TMP"
dnf download --resolve "${PKGS[@]}"

for rpm in ./*.rpm; do
  echo "   • unpacking $rpm"
  rpm2cpio "$rpm" | (cd "$LOCAL_PREFIX" && cpio -idmv)
done

# add pkgconfig, ld_library_path, cmake_prefix_path exports to ~/.bashrc
ENV_LINES=(
  'export PREFIX="${PREFIX:-$HOME/.local/usr}"'
  'export PATH="$HOME/.local/bin:$HOME/bin:$HOME/.cargo/bin:$PATH"'
  'export CPATH="$PREFIX/include${CPATH:+:}$CPATH"'
  'export LIBRARY_PATH="$PREFIX/lib64:$PREFIX/lib${LIBRARY_PATH:+:}$LIBRARY_PATH"'
  'export LD_LIBRARY_PATH="$PREFIX/lib64:$PREFIX/lib${LD_LIBRARY_PATH:+:}$LD_LIBRARY_PATH"'
  'export PKG_CONFIG_PATH="$PREFIX/lib64/pkgconfig:$PREFIX/lib/pkgconfig"'
  'export CMAKE_PREFIX_PATH="$PREFIX${CMAKE_PREFIX_PATH:+:}$CMAKE_PREFIX_PATH"'
)

echo "→ Updating ~/.bashrc with library paths…"
for line in "${ENV_LINES[@]}"; do
  if ! grep -Fxq "$line" "$HOME/.bashrc"; then
    echo "$line" >> "$HOME/.bashrc"
    echo "   ✓ Added: $line"
  else
    echo "   • Already present: $line"
  fi
done

# ────────────────────────────────────────────────────────────────────────────────
# 2) LLVM 20.1.0
# ────────────────────────────────────────────────────────────────────────────────
LLVM_VER="20.1.0"
PLATFORM="Linux-X64"
URL_BASE="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VER}"
LLVM_DEST="$HOME/tools/llvm"
LLVM_TARBALL="LLVM-${LLVM_VER}-${PLATFORM}.tar.xz"
UNPACK_DIR="${LLVM_DEST}/LLVM-${LLVM_VER}-${PLATFORM}"

mkdir -p "$LLVM_DEST"
if [ -d "$UNPACK_DIR" ]; then
    echo "✔ LLVM ${LLVM_VER} already installed at ${UNPACK_DIR}"
else
    echo "⇣ Downloading LLVM ${LLVM_VER}..."
    curl -L --fail -o "${LLVM_DEST}/${LLVM_TARBALL}" \
         "${URL_BASE}/${LLVM_TARBALL}"
    echo "⇳ Extracting ${LLVM_TARBALL}..."
    tar -xf "${LLVM_DEST}/${LLVM_TARBALL}" -C "$LLVM_DEST"
    rm "${LLVM_DEST}/${LLVM_TARBALL}"
    echo "✔ LLVM ${LLVM_VER} ready at ${UNPACK_DIR}"
fi

# ────────────────────────────────────────────────────────────────────────────────
# 3) GLFW 3.3.10
# ────────────────────────────────────────────────────────────────────────────────
GLFW_DIR="$HOME/tools/glfw"
if [ ! -d "$GLFW_DIR" ]; then
    echo "⇣ Cloning GLFW 3.3.10..."
    git clone --branch 3.3.10 --single-branch --depth 1 \
        https://github.com/glfw/glfw.git "$GLFW_DIR"
    (
      cd "$GLFW_DIR"
      mkdir -p build && cd build
      cmake .. \
        -DCMAKE_INSTALL_PREFIX="$GLFW_DIR" \
        -DBUILD_SHARED_LIBS=OFF \
        -DGLFW_BUILD_EXAMPLES=OFF \
        -DGLFW_BUILD_TESTS=OFF \
        -DGLFW_BUILD_DOCS=OFF
      cmake --build . --target install
    )
    echo "✔ GLFW installed at ${GLFW_DIR}"
else
    echo "✔ GLFW already present at ${GLFW_DIR}"
fi

# ────────────────────────────────────────────────────────────────────────────────
# 4) GLM 1.0.1
# ────────────────────────────────────────────────────────────────────────────────
GLM_DIR="$HOME/tools/glm"
if [ ! -d "$GLM_DIR" ]; then
    echo "⇣ Cloning GLM 1.0.1..."
    git clone --branch 1.0.1 --single-branch --depth 1 \
        https://github.com/g-truc/glm.git "$GLM_DIR"
    (
      cd "$GLM_DIR"
      mkdir -p build && cd build
      cmake .. \
        -DCMAKE_INSTALL_PREFIX="$GLM_DIR" \
        -DBUILD_SHARED_LIBS=OFF \
        -DGLM_BUILD_TESTS=OFF \
        -DGLM_BUILD_DOCS=OFF \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
      cmake --build . --target install
    )
    echo "✔ GLM installed at ${GLM_DIR}"
else
    echo "✔ GLM already present at ${GLM_DIR}"
fi

# ────────────────────────────────────────────────────────────────────────────────
# 5) Vulkan SDK 1.4.309.0
# ────────────────────────────────────────────────────────────────────────────────
VULKAN_SDK_VERSION="1.4.309.0"
VULKAN_TARBALL="vulkansdk-linux-x86_64-${VULKAN_SDK_VERSION}.tar.xz"
VULKAN_DOWNLOAD_URL="https://sdk.lunarg.com/sdk/download/${VULKAN_SDK_VERSION}/linux/${VULKAN_TARBALL}"
VULKAN_SDK_ROOT="$HOME/tools/vulkan/${VULKAN_SDK_VERSION}"

mkdir -p "$HOME/tools/vulkan"
if [ ! -d "$VULKAN_SDK_ROOT" ]; then
    echo "⇣ Downloading Vulkan SDK ${VULKAN_SDK_VERSION}..."
    curl -L --fail -o "$HOME/tools/vulkan/$VULKAN_TARBALL" \
         "$VULKAN_DOWNLOAD_URL"
    echo "⇳ Extracting Vulkan SDK..."
    tar -xJf "$HOME/tools/vulkan/$VULKAN_TARBALL" \
        -C "$HOME/tools/vulkan"
    rm "$HOME/tools/vulkan/$VULKAN_TARBALL"
    echo "✔ Vulkan SDK ready at ${VULKAN_SDK_ROOT}"
else
    echo "✔ Vulkan SDK already present at ${VULKAN_SDK_ROOT}"
fi

# ────────────────────────────────────────────────────────────────────────────────
# 6) Export Vulkan environment for CMake & runtime
# ────────────────────────────────────────────────────────────────────────────────
export VULKAN_SDK="$VULKAN_SDK_ROOT/x86_64"
export CMAKE_PREFIX_PATH="$VULKAN_SDK:$CMAKE_PREFIX_PATH"
export LD_LIBRARY_PATH="$VULKAN_SDK/lib:$LD_LIBRARY_PATH"
export PATH="$VULKAN_SDK/bin:$PATH"

echo "✅ All dependencies bootstrapped under $HOME/.local and $HOME/tools"
