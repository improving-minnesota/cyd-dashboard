#!/usr/bin/env python3
"""Regenerate the printable packaging assets for cyd-horizon.

The cover SVGs in this folder are the hand-maintained source of truth. This
script renders them to 300 DPI PNGs and rebuilds the cut-and-glue label
sheets (US Letter, landscape) with crop ticks and dashed cut borders.

    cyd-horizon/.venv/bin/python docs/packaging/build_packaging.py

Requires: cairosvg and pymupdf in the repo venv.

Print the PDFs at Actual Size / 100% scale — "Fit to page" shrinks the
labels below the intended physical cover dimensions.
"""

import pathlib

import cairosvg
import pymupdf

PKG = pathlib.Path(__file__).resolve().parent

# label physical size in points (72 pt = 1 in), PNG render size, sheet grid
COVERS = [
    {
        "name": "cyd-2.8-cover-4.5x2.25",
        "pdf": "cyd-2.8-packaging-labels.pdf",
        "px": (1350, 675),          # 4.50" x 2.25" at 300 DPI
        "label": (324, 162),        # 4.50" x 2.25" in points
        "xs": (52, 416),            # 2 columns
        "ys": (51, 233, 415),       # 3 rows -> 6 labels
    },
    {
        "name": "cyd-4.0-cover-5.0x2.75",
        "pdf": "cyd-4.0-packaging-labels.pdf",
        "px": (1500, 825),          # 5.00" x 2.75" at 300 DPI
        "label": (360, 198),        # 5.00" x 2.75" in points
        "xs": (24, 408),            # 2 columns
        "ys": (108, 330),           # 2 rows -> 4 labels
    },
]

PAGE_W, PAGE_H = 792, 612        # US Letter landscape, in points
DASH_GRAY = (0.6, 0.6, 0.6)
TICK_GRAY = (0.5, 0.5, 0.5)
TICK = 8                         # crop-tick length in points


def crop_ticks(page, r):
    """Short outward-only corner ticks; the dashed border carries the cut."""
    for cy in (r.y0, r.y1):
        page.draw_line(pymupdf.Point(r.x0 - TICK, cy), pymupdf.Point(r.x0, cy),
                       color=TICK_GRAY, width=0.6)
        page.draw_line(pymupdf.Point(r.x1, cy), pymupdf.Point(r.x1 + TICK, cy),
                       color=TICK_GRAY, width=0.6)
    for cx in (r.x0, r.x1):
        page.draw_line(pymupdf.Point(cx, r.y0 - TICK), pymupdf.Point(cx, r.y0),
                       color=TICK_GRAY, width=0.6)
        page.draw_line(pymupdf.Point(cx, r.y1), pymupdf.Point(cx, r.y1 + TICK),
                       color=TICK_GRAY, width=0.6)


def build_sheet(cover):
    png = PKG / f"{cover['name']}.png"
    lw, lh = cover["label"]
    doc = pymupdf.open()
    page = doc.new_page(width=PAGE_W, height=PAGE_H)
    for y in cover["ys"]:
        for x in cover["xs"]:
            r = pymupdf.Rect(x, y, x + lw, y + lh)
            page.insert_image(r, filename=str(png))
            page.draw_rect(r, color=DASH_GRAY, width=0.6, dashes="[3 3] 0")
            crop_ticks(page, r)
    out = PKG / cover["pdf"]
    doc.save(str(out), deflate=True)
    n = len(cover["xs"]) * len(cover["ys"])
    print(f"wrote {out.name} (1 page, {n} labels @ "
          f"{lw / 72:.2f}in x {lh / 72:.2f}in)")


def main():
    for cover in COVERS:
        svg = PKG / f"{cover['name']}.svg"
        png = PKG / f"{cover['name']}.png"
        w, h = cover["px"]
        cairosvg.svg2png(url=str(svg), write_to=str(png),
                         output_width=w, output_height=h)
        print(f"rendered {png.name} ({w}x{h})")
        build_sheet(cover)


if __name__ == "__main__":
    main()
