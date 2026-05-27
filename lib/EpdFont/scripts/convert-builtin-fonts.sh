#!/bin/bash

set -e

cd "$(dirname "$0")"
PYTHON_BIN="${PYTHON_BIN:-python3}"

THAI_INTERVALS=(--additional-intervals 0x0E00,0x0E7F)
CJK_INTERVALS=(
  --additional-intervals 0x2E80,0x303F
  --additional-intervals 0x3400,0x4DBF
  --additional-intervals 0x4E00,0x9FFF
)
CLOUDLOOP_FONT_SIZES=(12 14 16 18 20)
NOTOSANS_THAI_LOOPED_REGULAR_SIZES=(8 10 12 14 16 18 20)
NOTOSANS_THAI_LOOPED_UI_BOLD_SIZES=(10 12 14 16 18 20)
NOTOSANSSC_REGULAR_SIZES=(8 10 12)
NOTOSANSSC_BOLD_SIZES=(10 12)
# Mali — Poki's handwriting voice on boot/sleep screens (EN + TH only)
MALI_REGULAR_SIZES=(18)

# Bai Jamjuree — Thai/Latin sans for body + UI text. Per-size --advance-y values
# match CloudLoop's tight leading pattern (advanceY ≈ ascender + |descender| + 1)
# so the inter-line gap feels visually equivalent to EN body text on the same
# screen. The font's natural metrics height includes generous leading for Thai
# combining marks; this trims that buffer aggressively. If hardware testing
# shows top vowels/tone marks of one line clipping into the descender of the
# previous line, bump each value up by 2-3 px (or revert to natural metrics).
declare -a BAIJAMJUREE_REGULAR_SIZES=(8 10 12 14 16 18 20 22)
declare -a BAIJAMJUREE_BOLD_SIZES=(12 14 16 18 20 22)
declare -A BAIJAMJUREE_ADVANCE_Y=(
  [8]=23
  [10]=28
  [12]=33
  [14]=39
  [16]=44
  [18]=49
  [20]=54
  [22]=57   # natural metrics already tighter than the formula (would compute 59)
)

"$PYTHON_BIN" build_notosanssc_subset.py

for size in ${CLOUDLOOP_FONT_SIZES[@]}; do
  font_name="cloudloop_${size}_regular"
  font_path="../builtinFonts/source/CloudLoop/CloudLoop-Regular.otf"
  output_path="../builtinFonts/${font_name}.h"
  "$PYTHON_BIN" fontconvert.py $font_name $size $font_path --2bit --compress "${THAI_INTERVALS[@]}" > $output_path
  echo "Generated $output_path"
done

for size in ${NOTOSANS_THAI_LOOPED_REGULAR_SIZES[@]}; do
  font_name="notosansthailooped_${size}_regular"
  font_path="../builtinFonts/source/NotoSansThaiLooped/NotoSansThaiLooped-Regular.ttf"
  output_path="../builtinFonts/${font_name}.h"
  "$PYTHON_BIN" fontconvert.py $font_name $size $font_path --2bit --compress "${THAI_INTERVALS[@]}" > $output_path
  echo "Generated $output_path"
done

for size in ${NOTOSANS_THAI_LOOPED_UI_BOLD_SIZES[@]}; do
  font_name="notosansthailooped_${size}_bold"
  font_path="../builtinFonts/source/NotoSansThaiLooped/NotoSansThaiLooped-Bold.ttf"
  output_path="../builtinFonts/${font_name}.h"
  "$PYTHON_BIN" fontconvert.py $font_name $size $font_path --2bit --compress "${THAI_INTERVALS[@]}" > $output_path
  echo "Generated $output_path"
done

for size in ${NOTOSANSSC_REGULAR_SIZES[@]}; do
  font_name="notosanssc_${size}_regular"
  font_path="../builtinFonts/source/NotoSansSC/NotoSansSC-Subset-Regular.otf"
  output_path="../builtinFonts/${font_name}.h"
  "$PYTHON_BIN" fontconvert.py $font_name $size $font_path --2bit --compress "${CJK_INTERVALS[@]}" > $output_path
  echo "Generated $output_path"
done

for size in ${NOTOSANSSC_BOLD_SIZES[@]}; do
  font_name="notosanssc_${size}_bold"
  font_path="../builtinFonts/source/NotoSansSC/NotoSansSC-Subset-Bold.otf"
  output_path="../builtinFonts/${font_name}.h"
  "$PYTHON_BIN" fontconvert.py $font_name $size $font_path --2bit --compress "${CJK_INTERVALS[@]}" > $output_path
  echo "Generated $output_path"
done

for size in ${MALI_REGULAR_SIZES[@]}; do
  font_name="mali_${size}_regular"
  font_path="../builtinFonts/source/Mali/Mali-Regular.ttf"
  output_path="../builtinFonts/${font_name}.h"
  "$PYTHON_BIN" fontconvert.py $font_name $size $font_path --2bit --compress "${THAI_INTERVALS[@]}" > $output_path
  echo "Generated $output_path"
done

# Bai Jamjuree regenerator — only runs if the source TTFs are present (they are
# not committed to this repo). Hand-edited advanceY values in the existing
# headers will be overwritten by these explicit --advance-y flags if you regen.
BAIJAMJUREE_REGULAR_SRC="../builtinFonts/source/BaiJamjuree/BaiJamjuree-Regular.ttf"
BAIJAMJUREE_BOLD_SRC="../builtinFonts/source/BaiJamjuree/BaiJamjuree-Bold.ttf"
if [ -f "$BAIJAMJUREE_REGULAR_SRC" ]; then
  for size in "${BAIJAMJUREE_REGULAR_SIZES[@]}"; do
    font_name="baijamjuree_${size}_regular"
    output_path="../builtinFonts/${font_name}.h"
    advance_y="${BAIJAMJUREE_ADVANCE_Y[$size]}"
    "$PYTHON_BIN" fontconvert.py $font_name $size "$BAIJAMJUREE_REGULAR_SRC" --2bit --compress "${THAI_INTERVALS[@]}" --advance-y "$advance_y" > $output_path
    echo "Generated $output_path (advanceY=$advance_y)"
  done
fi
if [ -f "$BAIJAMJUREE_BOLD_SRC" ]; then
  for size in "${BAIJAMJUREE_BOLD_SIZES[@]}"; do
    font_name="baijamjuree_${size}_bold"
    output_path="../builtinFonts/${font_name}.h"
    advance_y="${BAIJAMJUREE_ADVANCE_Y[$size]}"
    "$PYTHON_BIN" fontconvert.py $font_name $size "$BAIJAMJUREE_BOLD_SRC" --2bit --compress "${THAI_INTERVALS[@]}" --advance-y "$advance_y" > $output_path
    echo "Generated $output_path (advanceY=$advance_y)"
  done
fi

echo ""
echo "Running compression verification..."
"$PYTHON_BIN" verify_compression.py ../builtinFonts/
