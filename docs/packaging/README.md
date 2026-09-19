# Packaging assets

Printable box-cover labels for cyd-dashboard devices. The SVGs are the
hand-maintained source of truth; PNGs and label-sheet PDFs are generated.

## Files

| File | Role |
| --- | --- |
| `cyd-2.8-cover-4.5x2.25.svg` | Source artwork, 2.8" CYD (2432S028R) cover |
| `cyd-2.8-cover-4.5x2.25.png` | Generated, 1350x675 = 300 DPI |
| `cyd-2.8-packaging-labels.pdf` | Generated, 1 page, 6 labels (2x3) |
| `cyd-4.0-cover-5.0x2.75.svg` | Source artwork, 4.0" CYD (E32R40T) cover |
| `cyd-4.0-cover-5.0x2.75.png` | Generated, 1500x825 = 300 DPI |
| `cyd-4.0-packaging-labels.pdf` | Generated, 1 page, 4 labels (2x2) |
| `build_packaging.py` | Regenerates all PNGs + label PDFs |

## Regenerating

```sh
cyd-dashboard/.venv/bin/python docs/packaging/build_packaging.py
```

Run from the repo root. Requires `cairosvg` and `pymupdf` in the repo venv.
The script renders each SVG at its exact 300 DPI pixel size and rebuilds the
US Letter landscape label sheets with dashed cut borders and outward crop
ticks. Label grids (in points, 72 pt = 1 in) live in the `COVERS` table at
the top of the script.

## Printing

Print the PDFs at **Actual Size / 100% scale** — "Fit to page" shrinks the
labels below the physical cover dimensions. Cut along the dashed borders.

## Artwork conventions (for agents editing the SVGs)

- Coordinate space = pixels at 300 DPI (2.8": `viewBox="0 0 1350 675"`,
  4.0": `viewBox="0 0 1500 825"`). Keep the two sizes in sync — they are
  near-identical layouts with different geometry constants.
- Layer order: borders → top bar (capsule left, edition right) → title →
  slogan → divider → radar scope → plane-bird emblem → vertical divider →
  bullets → bottom bar → footer.
- The radar scope is clipped to the middle-left panel (`middleSectionClip*`)
  so it never crosses the bottom separator or the vertical divider; its
  `ringFade*` gradients fade it out toward the text areas and bottom edge.
- The plane-bird emblem and radar share one `translate()` — move them
  together. Keep all artwork inside the separator lines with visible
  clearance (wings, pitot boom, afterburners must not touch lines).
- Text legibility at physical print size is the priority: title 88/104 pt,
  bullets 36/42 pt bold, slogan 31/36 pt, capsule 21/23 pt, edition and
  footer 23/25 pt (2.8"/4.0"). All text must stay inside the
  border/separator margins with breathing room.
- Title `cyd-dashboard` is three `<text>` runs (`cyd` gold, `-` and
  `dashboard` white) kerned so the hyphen is centered between the two `d`s.
- Futura/Helvetica font stacks only (print-safe system fonts).

The user-guide front-cover logo (`docs/user-guide/cyd-dashboard-guide-logo.svg`)
is a vertically-trimmed derivative of the 2.8" cover — same artwork rules
apply. See `docs/user-guide/README.md`.
