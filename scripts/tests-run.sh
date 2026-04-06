#!/usr/bin/env bash

set -u

PROGRAM="${1:-}"
TARGET_DIR="${2:-}"

total=0
passed=0
failed=0
errors=0

if [ -z "$PROGRAM" ] || [ -z "$TARGET_DIR" ]; then
  echo "Uso: $0 <ruta-al-ejecutable> <carpeta-de-tests>"
  exit 2
fi

if [ ! -x "$PROGRAM" ]; then
  echo "Error: no se encontro un ejecutable en $PROGRAM"
  exit 2
fi

if [ ! -d "$TARGET_DIR" ]; then
  echo "Error: la carpeta no existe: $TARGET_DIR"
  exit 2
fi

echo "Folder:"
echo "- $TARGET_DIR"
echo "Results:"

set_has_tests=0

for input_file in "$TARGET_DIR"/test*.in; do
  [ -f "$input_file" ] || continue
  set_has_tests=1

  test_name="$(basename "$input_file" .in)"
  expected_file="$TARGET_DIR/$test_name.expected"
  output_file="$TARGET_DIR/$test_name.out"
  error_file="$TARGET_DIR/$test_name.err"
  diff_file="$TARGET_DIR/$test_name.diff"
  total=$((total + 1))

  if [ ! -f "$expected_file" ]; then
    echo "- $test_name: ERRR"
    errors=$((errors + 1))
    continue
  fi

  "$PROGRAM" < "$input_file" > "$output_file" 2> "$error_file"
  program_status=$?

  if [ "$program_status" -ne 0 ]; then
    echo "- $test_name: ERRR"
    errors=$((errors + 1))
    continue
  fi

  diff -u "$expected_file" <(sed -n '1p' "$output_file") > "$diff_file"
  diff_status=$?

  if [ "$diff_status" -eq 0 ]; then
    echo "- $test_name: PASS"
    rm -f "$diff_file" "$error_file"
    passed=$((passed + 1))
  else
    echo "- $test_name: FAIL"
    failed=$((failed + 1))
  fi
done

if [ "$set_has_tests" -eq 0 ]; then
  echo "No se encontraron archivos testNNN.in en $TARGET_DIR"
  errors=$((errors + 1))
fi

echo
echo "Resumen: total=$total, pass=$passed, fail=$failed, error=$errors"

if [ "$failed" -eq 0 ] && [ "$errors" -eq 0 ]; then
  exit 0
fi

exit 1