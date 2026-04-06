#!/usr/bin/env bash

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LAB_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TARGET_DIR="${1:-$LAB_DIR/tests}"

if [ ! -d "$TARGET_DIR" ]; then
  echo "Error: la carpeta no existe: $TARGET_DIR"
  exit 2
fi

find "$TARGET_DIR" -type f \( -name '*.out' -o -name '*.err' -o -name '*.diff' \) -delete

echo "Archivos temporales eliminados en: $TARGET_DIR"