#!/usr/bin/env python3
"""Render DEVELOPER-USERGUIDE.md to the printable two-page user guide PDF.

Converts the markdown source to styled HTML and prints it to
DEVELOPER-USERGUIDE.pdf via headless Chrome, so the .md stays the single
source of truth for both GitHub and the printed handout. The guide is meant
to be duplex-printed on one Letter sheet (front = setup, back = reference);
the script reports the page count so drift past two pages is visible.

A `<!-- PAGEBREAK -->` comment in the markdown marks the front/back split.

Usage:
    cyd-dashboard/.venv/bin/python docs/user-guide/render_userguide.py
Requires: the `markdown` package in the venv (`python -m pip install markdown`)
and Google Chrome.
"""

import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent   # docs/user-guide/
REPO = ROOT.parents[1]                            # repo root
SRC = ROOT / "DEVELOPER-USERGUIDE.md"
OUT = ROOT / "DEVELOPER-USERGUIDE.pdf"
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

CSS = """
@page { size: Letter; margin: 0.55in 0.62in; }
body { font-family: -apple-system, BlinkMacSystemFont, "Helvetica Neue", Arial,
       sans-serif; font-size: 9.6pt; line-height: 1.38; color: #222; margin: 0; }
h1 { font-size: 17pt; margin: 0 0 4pt; color: #0b3d66;
     border-bottom: 2.5px solid #0b3d66; padding-bottom: 5pt; }
h2 { font-size: 11.5pt; margin: 8pt 0 3pt; color: #0b3d66; }
p { margin: 3pt 0; }
ul, ol { margin: 2pt 0 4pt; padding-left: 15pt; }
li { margin: 0 0 1.25pt; }
hr { border: none; border-top: 1px solid #ccc; margin: 6pt 0 3pt; }
.footer { font-size: 8pt; color: #777; }
img.dash { float: right; width: 41%; margin: 0 0 4pt 10pt;
           border: 1px solid #bbb; border-radius: 4px; }
.shots { display: flex; justify-content: space-between; margin: 3pt 0 5pt; }
.shots img { width: 28%; border: 1px solid #bbb; border-radius: 4px; }
.twocol { columns: 2; column-gap: 18pt; }
.twocol li { break-inside: avoid; }
.back { font-size: 9.0pt; line-height: 1.32; }
.back h2 { margin: 6pt 0 2.5pt; }
.back p { margin: 2pt 0; }
.back ul, .back ol { margin: 1pt 0 2pt; }
.back li { margin: 0 0 1pt; }
.back hr { margin: 4pt 0 2pt; }
.pagebreak { break-before: page; page-break-before: always; }
"""


def page_count(pdf_path):
    try:
        out = subprocess.run(
            ["mdls", "-name", "kMDItemNumberOfPages", "-raw", str(pdf_path)],
            capture_output=True, text=True, timeout=10)
        return int(out.stdout.strip())
    except Exception:
        data = pdf_path.read_bytes()
        return len(re.findall(rb"/Type\s*/Page[^s]", data))


def main():
    try:
        import markdown
    except ImportError:
        sys.exit("markdown package required: "
                 "cyd-dashboard/.venv/bin/python -m pip install markdown")
    if not pathlib.Path(CHROME).exists():
        sys.exit(f"Chrome not found at {CHROME}")

    # Regenerate the simulated dashboard screen first (real data from .env
    # creds when available). Non-fatal: a stale/missing image just shows the
    # alt text in the PDF.
    mock = ROOT / "render_dash_mock.py"
    if mock.exists():
        r = subprocess.run([sys.executable, str(mock)],
                           capture_output=True, text=True, timeout=120)
        print(r.stdout.strip() or r.stderr.strip()[:300])

    md = SRC.read_text()
    # Split on the pagebreak marker; the back page gets a slightly smaller
    # type scale via the .back wrapper.
    parts = re.split(r"<!--\s*PAGEBREAK\s*-->", md, maxsplit=1)
    if len(parts) == 2:
        md = (parts[0] + '<div class="pagebreak"></div>\n'
              '<div class="back" markdown="1">\n' + parts[1] + "\n</div>")
    body = markdown.markdown(md, extensions=["sane_lists", "md_in_html"])
    html = f"<!doctype html><meta charset='utf-8'><style>{CSS}</style>{body}"

    # Write into the repo's git-ignored build/; image srcs sit next to the
    # markdown, so point them at the real files.
    html = re.sub(r'src="(DEVELOPER-USERGUIDE-[^"]+\.png)"',
                  lambda m: f'src="{ROOT}/{m.group(1)}"', html)
    build = REPO / "build"
    build.mkdir(exist_ok=True)
    html_path = build / "userguide.html"
    html_path.write_text(html)

    subprocess.run(
        [CHROME, "--headless=new", "--disable-gpu", "--disable-extensions",
         "--no-pdf-header-footer", f"--print-to-pdf={OUT}",
         f"file://{html_path}"],
        check=True, capture_output=True, timeout=60)

    pages = page_count(OUT)
    print(f"wrote {OUT.name} ({pages} page(s))"
          + ("" if pages == 2 else " — expected 2, adjust content or CSS"))


if __name__ == "__main__":
    main()
