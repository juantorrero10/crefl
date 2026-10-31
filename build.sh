#!/usr/bin/env bash
set -euo pipefail

# ---- Config ----
OUT_DIR="./out"
SRC_DIR="src"
GEN_DIR="crefl_gen"
LIB_PATH="build/lib/libcrefl.a"   # <- adjust if needed

echo "==> Building project"
if [[ $# -gt 0 ]]; then 
    make static -j4 OPT="$1"
else
    make static -j4 OPT=3
fi

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

# ---- Copy static library ----
if [ -f "$LIB_PATH" ]; then
    echo "==> Copying static library"
    cp "$LIB_PATH" "$OUT_DIR/lib/"
else
    echo "!! Warning: $LIB_PATH not found, skipping"
fi

# ---- Copy headers (from src/include/public to out/include/crefl) ----
echo "==> Copying public headers"
if [ -d "$SRC_DIR/include/public" ]; then
    mkdir -p "$OUT_DIR/include/crefl"
    # The /. at the end ensures we copy the contents of the folder, not the folder itself
    cp -r "$SRC_DIR/include/public/". "$OUT_DIR/include/crefl/"
else
    echo "!! Warning: $SRC_DIR/include/public not found, skipping headers"
fi

echo "==> Done"
echo "Output available at: $OUT_DIR"