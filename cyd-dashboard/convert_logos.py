#!/usr/bin/env python3
"""Convert the airline logo images in ./airline-logos into RGB565 bitmaps.

Two output modes:
  --out-dir DIR   Write one <ICAO>.bin file per logo (preferred; logos live in
                  a dedicated LittleFS partition and are loaded at runtime, so
                  firmware never embeds them).
  --header PATH   (default, legacy) Write the old C header with a PROGMEM array
                  plus a lookup table keyed by ICAO airline code.

For each image (PNG; sources are local-only, git-ignored) it fits the icon into
a fixed WxH box (preserving aspect ratio, centered) and converts to 16-bit
RGB565.

.bin file layout (little-endian):
  "LGO1" magic (4 bytes) | w (u16) | h (u16) | transparent (u16) | reserved (2)
  | w*h*2 bytes of RGB565

Usage:
    python convert_logos.py --out-dir build/logos
    python convert_logos.py --header airline_logos.h
"""
import os
import sys
import argparse
import struct
from PIL import Image

SRC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "airline-logos")
DEFAULT_HDR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "airline_logos.h")
LOGO_MAGIC = b"LGO1"

# Box matches the on-screen size (drawn 1:1, no runtime upscaling) so a single
# high-quality downsample from the source PNG preserves detail, instead of
# downsampling small then blowing back up with blocky nearest-neighbor scaling.
BOX_W = 72   # target box width  (= on-screen logo width)
BOX_H = 48   # target box height (= on-screen logo height)
TRANSPARENT = 0xF81F   # magenta; TFT_eSPI skips this color when pushing

# (filename keyword, ICAO code, nice name). The keyword must match exactly one
# source file (case-insensitive substring). Keep keywords specific/ordered so
# ambiguous names resolve uniquely (e.g. UPS uses "Parcel" before "United").
AIRLINES = [
    ("ANA Holdings",    "ANA", "ANA (All Nippon Airways)"),
    ("Aer Lingus",      "EIN", "Aer Lingus"),
    ("Aerom",           "AMX", "Aeromexico"),
    ("Air Canada",      "ACA", "Air Canada"),
    ("Air China",       "CCA", "Air China"),
    ("Air France",      "AFR", "Air France"),
    ("AirAsia",         "AXM", "AirAsia"),
    ("Alaska",          "ASA", "Alaska Airlines"),
    ("Allegiant",       "AAY", "Allegiant Air"),
    ("American",        "AAL", "American Airlines"),
    ("British",         "BAW", "British Airways"),
    ("Cathay",          "CPA", "Cathay Pacific"),
    ("China Eastern",   "CES", "China Eastern Airlines"),
    ("China Southern",  "CSN", "China Southern Airlines"),
    ("Copa",            "CMP", "Copa Airlines"),
    ("Delta",           "DAL", "Delta Air Lines"),
    ("Envoy",           "ENY", "Envoy Air"),
    ("Etihad",          "ETD", "Etihad Airways"),
    ("Eurowings",       "EWG", "Eurowings"),
    ("FedEx",           "FDX", "FedEx Express"),
    ("Finnair",         "FIN", "Finnair"),
    ("Frontier",        "FFT", "Frontier Airlines"),
    ("Hainan",          "CHH", "Hainan Airlines"),
    ("Hawaiian",        "HAL", "Hawaiian Airlines"),
    ("Iberia",          "IBE", "Iberia"),
    ("Icelandair",      "ICE", "Icelandair"),
    ("InterGlobe",      "IGO", "IndiGo (InterGlobe)"),
    ("Japan",           "JAL", "Japan Airlines"),
    ("Jetblue",         "JBU", "JetBlue Airways"),
    ("KLM",             "KLM", "KLM Royal Dutch"),
    ("Korean",          "KAL", "Korean Air"),
    ("Lufthansa",       "DLH", "Lufthansa"),
    ("Norwegian",       "NAX", "Norwegian Air Shuttle"),
    ("Qantas",          "QFA", "Qantas Airways"),
    ("Qatar",           "QTR", "Qatar Airways"),
    ("Ryanair",         "RYR", "Ryanair"),
    ("Scandinavian",    "SAS", "Scandinavian Airlines (SAS)"),
    ("Singapore",       "SIA", "Singapore Airlines"),
    ("SkyWest",         "SKW", "SkyWest Airlines"),
    ("Southwest",       "SWA", "Southwest Airlines"),
    ("Sun Country",     "SCX", "Sun Country Airlines"),
    ("Swiss",           "SWR", "Swiss International Air Lines"),
    ("TACA",            "AVA", "Avianca (TACA)"),
    ("TAM",             "TAM", "TAM (LATAM)"),
    ("TAP",             "TAP", "TAP Air Portugal"),
    ("Emirates",        "UAE", "Emirates"),
    ("Turkish",         "THY", "Turkish Airlines"),
    ("United Airlines", "UAL", "United Airlines"),
    ("Parcel",          "UPS", "UPS Airlines"),
    ("Virgin",          "VIR", "Virgin Atlantic"),
    ("Vueling",         "VLG", "Vueling"),
    ("WestJet",         "WJA", "WestJet"),
    ("Wizz",            "WZZ", "Wizz Air"),
    ("easyJet",         "EZY", "easyJet"),
]

def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def load_image_to_rgba(path, box_w, box_h):
    """Open an image and fit it into a WxH transparent box (aspect preserved)."""
    img = Image.open(path).convert("RGBA")
    iw, ih = img.size
    scale = min(box_w / iw, box_h / ih)
    nw, nh = max(1, int(round(iw * scale))), max(1, int(round(ih * scale)))
    img = img.resize((nw, nh), Image.LANCZOS)

    box = Image.new("RGBA", (box_w, box_h), (0, 0, 0, 0))
    box.paste(img, ((box_w - nw) // 2, (box_h - nh) // 2), img)
    return box

def to_rgb565_array(rgba):
    """Convert RGBA -> list of RGB565 uint16, transparent -> TRANSPARENT color."""
    w, h = rgba.size
    px = rgba.load()
    out = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < 16:
                out.append(TRANSPARENT)
            else:
                out.append(rgb565(r, g, b))
    return out

def write_logo_files(out_dir, logos, box_w, box_h):
    """Write one <ICAO>.bin per logo: 12-byte header + raw RGB565 bytes."""
    os.makedirs(out_dir, exist_ok=True)
    total = 0
    for icao, name, arr in logos:
        with open(os.path.join(out_dir, f"{icao}.bin"), "wb") as f:
            f.write(LOGO_MAGIC)
            f.write(struct.pack("<HHH", box_w, box_h, TRANSPARENT))
            f.write(struct.pack("<H", 0))  # reserved
            f.write(struct.pack(f"<{box_w * box_h}H", *arr))
        total += len(arr) * 2
        print(f"OK {icao}: {name}  ({box_w}x{box_h}, {len(arr)} px)")
    print(f"\nWrote {len(logos)} logos as <ICAO>.bin to {out_dir} "
          f"({total // 1024} KB raw)")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--out-dir", metavar="DIR",
                        help="write <ICAO>.bin logo files into DIR (for the "
                             "LittleFS logos partition)")
    parser.add_argument("--header", metavar="PATH", nargs="?",
                        const=DEFAULT_HDR, default=DEFAULT_HDR,
                        help="legacy: write the C header (default output)")
    args = parser.parse_args()

    src_files = [f for f in os.listdir(SRC_DIR)
                 if not f.startswith(".") and f.lower() != "readme.md"]

    found = []
    errors = []
    for key, icao, name in AIRLINES:
        matches = [f for f in src_files if key.lower() in f.lower()]
        if not matches:
            errors.append(f"NO SOURCE for {icao} ({name}): keyword '{key}'")
            continue
        if len(matches) > 1:
            errors.append(f"AMBIGUOUS for {icao} ({name}): keyword '{key}' -> {matches}")
            continue
        path = os.path.join(SRC_DIR, matches[0])
        try:
            rgba = load_image_to_rgba(path, BOX_W, BOX_H)
        except Exception as e:
            errors.append(f"ERROR {icao} ({name}): {path}: {e}")
            continue
        arr = to_rgb565_array(rgba)
        found.append((icao, name, arr))
        print(f"OK {icao}: {name}  ({BOX_W}x{BOX_H}, {len(arr)} px)")

    if errors:
        print("\n".join(errors))
        return 2
    if not found:
        print("No logos converted.")
        return 1

    if args.out_dir:
        return write_logo_files(args.out_dir, found, BOX_W, BOX_H)

    lines = []
    lines.append("// Auto-generated airline logos (RGB565, PROGMEM). Do not edit.")
    lines.append(f"// {BOX_W}x{BOX_H}, transparent color 0x{TRANSPARENT:04X}.")
    lines.append("#pragma once")
    lines.append("#include <pgmspace.h>")
    lines.append("")
    for icao, name, arr in found:
        sym = f"logo_{icao}"
        lines.append(f"// {name}")
        lines.append(f"static const uint16_t {sym}[{BOX_W * BOX_H}] PROGMEM = {{")
        row = []
        for i, v in enumerate(arr):
            row.append(f"0x{v:04X}")
            if len(row) == 12:
                lines.append("  " + ",".join(row) + ",")
                row = []
        if row:
            lines.append("  " + ",".join(row) + ",")
        lines.append("};")
        lines.append("")
    # lookup table
    lines.append(f"struct AirlineLogo {{ const char* icao; const uint16_t* data; int w; int h; }};")
    lines.append(f"static const AirlineLogo kAirlineLogos[] = {{")
    for icao, name, arr in found:
        lines.append(f"  {{\"{icao}\", logo_{icao}, {BOX_W}, {BOX_H}}},  // {name}")
    lines.append("};")
    lines.append(f"static const int kNumAirlineLogos = sizeof(kAirlineLogos) / sizeof(kAirlineLogos[0]);")
    lines.append("")

    with open(args.header, "w") as f:
        f.write("\n".join(lines))
    print(f"\nWrote {args.header} ({len(found)} logos, {sum(len(a) for _,_,a in found)*2//1024} KB)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
