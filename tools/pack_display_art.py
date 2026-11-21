"""Rebuild the flash-ready StickS3 artwork from web/assets (requires Pillow)."""

from pathlib import Path
import struct

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
SIZE = (112, 68)
# Tile order matches Artwork in display.cpp.
NAMES = ("frostling", "boss", "duel", "portal", "shrine", "fire", "frost", "ward", "bloom")


def tile(name):
    with Image.open(ROOT / "web" / "assets" / f"{name}.webp") as source:
        art = source.convert("RGB")
    if name in ("fire", "frost", "ward", "bloom"):
        # Keep the entire glyph visible in the landscape frame.
        frame = Image.new("RGB", SIZE, art.getpixel((0, 0)))
        icon = ImageOps.contain(art, SIZE, Image.Resampling.LANCZOS)
        frame.paste(icon, ((SIZE[0] - icon.width) // 2, 0))
    else:
        focus = (0.93, 0.5) if name == "frostling" else (0.5, 0.4)
        frame = ImageOps.fit(art, SIZE, Image.Resampling.LANCZOS, centering=focus)
    return b"".join(struct.pack(">H", (r >> 3) << 11 | (g >> 2) << 5 | b >> 3)
                    for r, g, b in frame.getdata())


if __name__ == "__main__":
    target = ROOT / "assets" / "rift_tiles.bin"
    target.parent.mkdir(exist_ok=True)
    target.write_bytes(b"".join(tile(name) for name in NAMES))
