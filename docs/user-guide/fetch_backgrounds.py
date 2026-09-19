#!/usr/bin/env python3
"""Fetch the user guide's page background images once and save them locally.

Uses Pollinations.ai (free, no API key) to generate three candidate
backgrounds for the printable guide. Images are saved next to this script so
PDF renders never hit the network — re-run with --refresh (optionally with
--seed N) only when we want a different look.

The free endpoint stamps a small `pollinations.ai` watermark in the
bottom-right corner; the script crops the bottom strip so it never reaches
the PDF (background-size: cover absorbs the slight aspect change).

Usage:
    cyd-horizon/.venv/bin/python docs/user-guide/fetch_backgrounds.py [--refresh] [--seed N]
"""

import io
import pathlib
import sys
import time
import urllib.parse
import urllib.request

from PIL import Image, ImageFilter

ROOT = pathlib.Path(__file__).resolve().parent

# Letter page at ~150 dpi.
W, H = 1275, 1650
CROP_BOTTOM = 110   # removes the pollinations.ai watermark strip
BLUR = 30           # smooth out photo grain/objects — it's a backdrop, not art
WASH = 0.62         # blend this far toward white to keep text legible

CONCEPTS = {
    "bg-lines.png":
        "Pale sky blue gradient background filling the entire frame edge "
        "to edge with a few very faint thin concentric radar range rings "
        "and a faint compass arc near the edges, mostly empty, high "
        "negative space, very low contrast, subtle minimal wallpaper for "
        "text overlay, no objects, no watermark, no words, no letters, "
        "no text",
    "bg-leaf.png":
        "Soft pale blue sky gradient background filling the entire frame "
        "edge to edge with a few very faint wispy blurred white clouds "
        "near the edges only, mostly empty middle, high negative space, "
        "very low contrast, minimal wallpaper for text overlay, no "
        "objects, no watermark, no words, no letters, no text",
    "bg-blur.png":
        "Soft dreamy pale blue sky filling the entire frame edge to "
        "edge, a single blurred commercial airplane silhouette seen from "
        "below in the upper third and faint thin concentric radar rings "
        "in the lower corner, heavy gaussian blur, pastel, low contrast, "
        "high negative space, minimal wallpaper for text overlay, no "
        "watermark, no words, no letters, no text",
}


def fetch(url, dest, blur=BLUR, wash=WASH):
    req = urllib.request.Request(url, headers={"User-Agent": "cyd-userguide/1.0"})
    with urllib.request.urlopen(req, timeout=120) as r:
        img = Image.open(io.BytesIO(r.read())).convert("RGB")
    img = img.crop((0, 0, img.width, img.height - CROP_BOTTOM))
    img = img.filter(ImageFilter.GaussianBlur(blur))
    white = Image.new("RGB", img.size, (255, 255, 255))
    Image.blend(img, white, wash).save(dest)


def main():
    refresh = "--refresh" in sys.argv
    seed = 7
    if "--seed" in sys.argv:
        seed = int(sys.argv[sys.argv.index("--seed") + 1])

    for name, prompt in CONCEPTS.items():
        dest = ROOT / name
        if dest.exists() and not refresh:
            print(f"{name}: exists (use --refresh to regenerate)")
            continue
        url = ("https://image.pollinations.ai/prompt/" + urllib.parse.quote(prompt)
               + f"?width={W}&height={H}&seed={seed}&nologo=true&model=flux")
        try:
            # The cover art keeps a bit more of its motif (less wash) so the
            # blurred plane/radar hints stay visible.
            if name == "bg-blur.png":
                fetch(url, dest, blur=18, wash=0.45)
            else:
                fetch(url, dest)
            print(f"{name}: wrote {dest.stat().st_size // 1024} KB")
        except Exception as e:
            print(f"{name}: FAILED ({e})")
        time.sleep(2)   # be polite to the free endpoint


if __name__ == "__main__":
    main()
