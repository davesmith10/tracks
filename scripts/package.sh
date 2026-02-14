#!/usr/bin/env bash
#
# package.sh — Build TRACKS and create a self-contained tarball
#
# Usage: ./scripts/package.sh [VERSION]
#   VERSION defaults to "0.1.0"
#
set -euo pipefail

VERSION="${1:-0.1.0}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
PACKAGE_NAME="tracks-${VERSION}-linux-x86_64"
STAGE_DIR="$BUILD_DIR/$PACKAGE_NAME"

# System libs to exclude (present on any Linux)
SYSTEM_LIBS=(
    "linux-vdso.so"
    "ld-linux-x86-64.so"
    "libc.so"
    "libm.so"
    "libpthread.so"
    "libdl.so"
    "librt.so"
    "libgcc_s.so"
    "libstdc++.so"
    "libresolv.so"
    "libnss_"
    "libnsl.so"
)

is_system_lib() {
    local lib="$1"
    for pattern in "${SYSTEM_LIBS[@]}"; do
        if [[ "$lib" == *"$pattern"* ]]; then
            return 0
        fi
    done
    return 1
}

echo "=== TRACKS packager v${VERSION} ==="

# 1. Build (Release mode)
echo ""
echo "--- Building (Release) ---"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j"$(nproc)"

# 2. Set up staging directory
echo ""
echo "--- Staging ---"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/lib"

# 3. Copy binaries
cp "$BUILD_DIR/tracks" "$STAGE_DIR/"
cp "$BUILD_DIR/tracks-recv" "$STAGE_DIR/"

# 4. Collect and copy shared library dependencies
echo ""
echo "--- Collecting dependencies ---"
collect_deps() {
    local binary="$1"
    ldd "$binary" 2>/dev/null | while read -r line; do
        # Parse lines like: libfoo.so.1 => /usr/lib64/libfoo.so.1 (0x...)
        local lib_path
        lib_path=$(echo "$line" | awk '{print $3}')
        local lib_name
        lib_name=$(echo "$line" | awk '{print $1}')

        # Skip lines without a path (e.g. linux-vdso, ld-linux)
        if [[ -z "$lib_path" || "$lib_path" == "not" || ! -f "$lib_path" ]]; then
            continue
        fi

        if is_system_lib "$lib_name"; then
            continue
        fi

        echo "$lib_path"
    done
}

# Collect deps from both binaries, deduplicate
DEPS=$(
    { collect_deps "$STAGE_DIR/tracks"; collect_deps "$STAGE_DIR/tracks-recv"; } \
    | sort -u
)

for dep in $DEPS; do
    cp -L "$dep" "$STAGE_DIR/lib/"
    echo "  $(basename "$dep")"
done

echo "  Total: $(echo "$DEPS" | wc -l) libraries"

# 5. Patch RPATH on binaries
echo ""
echo "--- Patching RPATH ---"
patchelf --set-rpath '$ORIGIN/lib' "$STAGE_DIR/tracks"
patchelf --set-rpath '$ORIGIN/lib' "$STAGE_DIR/tracks-recv"
echo "  tracks: $(patchelf --print-rpath "$STAGE_DIR/tracks")"
echo "  tracks-recv: $(patchelf --print-rpath "$STAGE_DIR/tracks-recv")"

# 6. Copy supporting files
echo ""
echo "--- Copying supporting files ---"
mkdir -p "$STAGE_DIR/proto" "$STAGE_DIR/config"
cp "$PROJECT_DIR/proto/tracks.proto" "$STAGE_DIR/proto/"
cp "$PROJECT_DIR/config/tracks-default.yaml" "$STAGE_DIR/config/"
cp "$PROJECT_DIR/README.md" "$STAGE_DIR/"
cp "$PROJECT_DIR/PROTOBUF.md" "$STAGE_DIR/"
cp "$PROJECT_DIR/CLIENT.md" "$STAGE_DIR/"
[ -f "$PROJECT_DIR/LICENSE" ] && cp "$PROJECT_DIR/LICENSE" "$STAGE_DIR/"

# 7. Create tarball
echo ""
echo "--- Creating tarball ---"
cd "$BUILD_DIR"
tar czf "${PACKAGE_NAME}.tar.gz" "$PACKAGE_NAME"

TARBALL="$BUILD_DIR/${PACKAGE_NAME}.tar.gz"
echo ""
echo "=== Done ==="
echo "Tarball: $TARBALL"
echo "Size: $(du -h "$TARBALL" | cut -f1)"
echo ""
echo "To verify:"
echo "  tar xzf $TARBALL"
echo "  cd $PACKAGE_NAME"
echo "  ldd ./tracks  # all libs should resolve from ./lib/"
echo "  ./tracks --help"
