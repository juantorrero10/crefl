#!/usr/bin/env bash
set -euo pipefail

# ---- Config ----
OUT_DIR="${1:-./extracted}"
SRC_DIR="src"
GEN_DIR="crefl_gen"
LIB_PATH="build/lib/libcrefl.a"   # <- adjust if needed

echo "==> Building project"
make static -j4 OPT=3

echo "==> Preparing output directory: $OUT_DIR"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
mkdir -p "$OUT_DIR/lib"

# ---- Copy generator ----
if [ -d "$GEN_DIR" ]; then
    echo "==> Copying generator ($GEN_DIR)"
    cp -r "$GEN_DIR" "$OUT_DIR/"
else
    echo "!! Warning: $GEN_DIR not found, skipping"
fi

# ---- Copy shared library ----
if [ -f "$LIB_PATH" ]; then
    echo "==> Copying shared library"
    cp "$LIB_PATH" "$OUT_DIR/lib/"
else
    echo "!! Warning: $LIB_PATH not found, skipping"
fi

# ---- Copy headers (preserve structure, skip _*) ----
echo "==> Copying public headers"

while IFS= read -r -d '' header; do
    rel="${header#$SRC_DIR/}"
    dest_dir="$OUT_DIR/$(dirname "$rel")"
    mkdir -p "$dest_dir"
    cp "$header" "$dest_dir/"
done < <(find "$SRC_DIR" -type f -name "*.h" ! -name "_*" -print0)

echo "==> Done"
echo "Output available at: $OUT_DIR"
