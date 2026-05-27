#!/usr/bin/env python3
"""Convert the 'I'm going to sleep' bunny+bubble+sparkle source image into a
1-bit, pre-rotated C byte array for the X3 GfxRenderer's drawImage() path.

The baked English text is erased from the source so firmware can overlay any
localized string (EN/TH) in Poki's handwriting font at runtime.

Pipeline:
  source PNG (1152x249 RGBA)
    -> flatten onto white
    -> erase text rectangle (bunny + bubble frame + sparkle remain)
    -> resize to TARGET_APPEARANCE_W x TARGET_APPEARANCE_H (logical portrait dims)
    -> threshold to 1-bit (crisp line art)
    -> rotate 90 deg CCW (matches the PokiInkBrand320Rot pre-rotation convention
       so the image renders upright through GfxRenderer::drawImage on X3)
    -> pack as C array, 1 bit per pixel, MSB first, white=1 / black=0

BOTH the appearance dimensions and the stored dimensions must be divisible by 8,
because the low-level EInkDisplay::drawImage uses integer division
`imageWidthBytes = w / 8`. If w is not a multiple of 8, rows are truncated and
the image renders as diagonal stripes.
"""
from __future__ import annotations

import os

from PIL import Image


SOURCE = "/Users/nakarinkochapond/Documents/Xteink X3 FW/I'm going to sleep.png"

# Logical-portrait appearance (how wide/tall the popup looks on-screen).
# Both must be multiples of 8 — see module docstring.
# 496 x 104 keeps the ~4.6:1 aspect ratio of the source (1152x249).
TARGET_APPEARANCE_W = 496
TARGET_APPEARANCE_H = 104

# Text rectangle in source coordinates (x0, y0, x1, y1).
# The speech bubble contents — between the bunny and the sparkle — are erased so
# firmware can draw a translated message with the Poki font.
# Source image is 1152 x 249; preserve sparkle at x>=945.
#
# IMPORTANT: the bubble frame is a thin line — pixel-probe of the source gives:
#   top border line     y ≈ 81-84
#   baked English text  y ≈ 118-220
#   bottom border line  y ≈ 231-234
# Keep y0 BELOW the top border (≥90) and y1 ABOVE the bottom border (≤225) so
# the erase rectangle clears only the text, not the frame itself. Getting this
# wrong makes the rendered popup look like its top edge is missing.
ERASE_BOX = (250, 90, 945, 225)

# Luminance threshold used when converting to 1-bit. The source image has
# solid black line art on a light beige background; a high threshold keeps
# the bubble outline crisp while letting the soft bunny shading vanish to
# white so the result doesn't look noisy on e-ink.
BINARY_THRESHOLD = 200

OUTPUT_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "src",
    "images",
)
OUTPUT_NAME = "PokiSleepPopup500Rot"
OUTPUT_PNG = os.path.join(OUTPUT_DIR, f"{OUTPUT_NAME}.png")
OUTPUT_HEADER = os.path.join(OUTPUT_DIR, f"{OUTPUT_NAME}.h")


def flatten_and_erase(src_path: str) -> Image.Image:
    img = Image.open(src_path).convert("RGBA")
    white = Image.new("RGBA", img.size, (255, 255, 255, 255))
    white.paste(img, mask=img.split()[3])
    # Paint the text bounding box pure white so the renderer can overlay
    # localized text without fighting the source's English letters.
    erase = Image.new("RGBA", (ERASE_BOX[2] - ERASE_BOX[0], ERASE_BOX[3] - ERASE_BOX[1]),
                      (255, 255, 255, 255))
    white.paste(erase, (ERASE_BOX[0], ERASE_BOX[1]))
    return white


def to_1bit_rotated(img: Image.Image) -> Image.Image:
    """Resize to the target appearance size, threshold to 1-bit, then pre-rotate
    90 deg COUNTER-CLOCKWISE so GfxRenderer::drawImage renders the popup upright
    in logical portrait view.

    Uses a plain threshold instead of Floyd-Steinberg dithering so thin bubble
    outlines and bunny strokes stay solid (dithering turned them into sparse
    checkerboards that look muddy on the e-ink panel).

    Rotation direction rationale: on X3 the Portrait path in
    GfxRenderer::drawImage writes bitmap data straight into the landscape
    framebuffer without rotating the bits. The framebuffer is then displayed
    90 deg clockwise relative to logical portrait, so the stored bitmap must be
    pre-rotated 90 deg counter-clockwise for the final image to read upright —
    same convention PokiInkBrand320Rot follows.
    """
    resized = img.convert("RGB").resize((TARGET_APPEARANCE_W, TARGET_APPEARANCE_H), Image.LANCZOS)
    gs = resized.convert("L")
    mono = gs.point(lambda v: 255 if v >= BINARY_THRESHOLD else 0, mode="1")
    # PIL's ROTATE_90 = 90 deg counter-clockwise.
    rotated = mono.transpose(Image.ROTATE_90)
    return rotated


def pack_c_array(img: Image.Image, name: str) -> str:
    """Pack as MSB-first 1-bit-per-pixel bytes. White pixel = 1, black = 0
    (matches the existing PokiInkBrand320Rot convention where 0xFF = all white)."""
    pixels = img.load()
    w, h = img.size
    packed = bytearray()
    for y in range(h):
        for x0 in range(0, w, 8):
            byte = 0
            for b in range(8):
                x = x0 + b
                if x < w:
                    # PIL mode '1' returns 0 for black, 255 for white
                    bit = 1 if pixels[x, y] >= 128 else 0
                    byte |= bit << (7 - b)
            packed.append(byte)
    lines = []
    lines.append("#pragma once")
    lines.append("#include <cstdint>")
    lines.append("")
    lines.append(f"// Source: I'm going to sleep.png -> erased text area -> threshold 1-bit")
    lines.append(f"// Stored size (drawImage w x h): {w}x{h}")
    lines.append(f"// Logical portrait appearance: {TARGET_APPEARANCE_W}x{TARGET_APPEARANCE_H}")
    lines.append(f"// White = 1, Black = 0 (matches PokiInkBrand320Rot packing)")
    lines.append(f"static const uint8_t {name}[] = {{")
    for i in range(0, len(packed), 12):
        chunk = ", ".join(f"0x{b:02x}" for b in packed[i : i + 12])
        trailing = "," if i + 12 < len(packed) else ""
        lines.append(f"    {chunk}{trailing}")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    if TARGET_APPEARANCE_W % 8 or TARGET_APPEARANCE_H % 8:
        raise SystemExit(
            f"TARGET_APPEARANCE_W ({TARGET_APPEARANCE_W}) and TARGET_APPEARANCE_H "
            f"({TARGET_APPEARANCE_H}) must both be divisible by 8 — "
            f"EInkDisplay::drawImage uses integer `w / 8` for row stride.")
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    flat = flatten_and_erase(SOURCE)
    rotated = to_1bit_rotated(flat)

    # Preview PNG (rotated BACK to logical for human inspection).
    preview = rotated.transpose(Image.ROTATE_270)  # undo the CCW pre-rotation for preview
    preview.save(OUTPUT_PNG)

    header = pack_c_array(rotated, OUTPUT_NAME)
    with open(OUTPUT_HEADER, "w") as f:
        f.write(header)

    stored_w, stored_h = rotated.size
    expected_bytes = stored_w // 8 * stored_h
    print(f"Wrote {OUTPUT_HEADER}")
    print(f"Wrote preview {OUTPUT_PNG}")
    print(f"Stored size: {stored_w}x{stored_h}  (drawImage w x h)")
    print(f"Appearance:  {TARGET_APPEARANCE_W}x{TARGET_APPEARANCE_H}  (logical portrait)")
    print(f"Expected byte count: {expected_bytes}")


if __name__ == "__main__":
    main()
