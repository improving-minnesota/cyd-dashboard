#!/usr/bin/env python3
"""Flash a prebuilt binary to a CYD over USB (arduino-cli upload).

Detects which serial port holds the requested board variant via
scripts/detect_boards.py (the firmware's "[boot] board=" marker), then runs
`arduino-cli upload` with that board's max upload baud.

Use this for the FIRST dev flash, when OTA flags change, or when the board is
unreachable on WiFi - day-to-day iteration should use ota_push.py instead.
Compile first (see DEVELOPER.md); this only uploads an existing build dir.

Usage (from the repo root):
  cyd-dashboard/.venv/bin/python scripts/flash.py --board e32r40t
      [--dir build/release-e32r40t] [--port /dev/cu.usbserial-XXXX]
"""

import argparse
import glob
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import detect_boards

FQBN = "esp32:esp32:jczn_2432s028r:PartitionScheme=custom"

# Per-variant defaults: build dir and the upload baud the board's USB-serial
# bridge tolerates. The E32R40T's CH340 drops mid-transfer at 921600.
BOARDS = {
    "2432s028r": {"dir": "build/release",         "speed": 115200},
    "e32r40t":   {"dir": "build/release-e32r40t", "speed": 460800},
}


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--board", choices=sorted(BOARDS),
                    help="board variant (default: inferred from --dir, or the "
                         "sole connected board)")
    ap.add_argument("--dir", help="build output dir (default: per-board)")
    ap.add_argument("--port", help="serial port (default: auto-detect)")
    ap.add_argument("--speed", type=int, help="upload baud (default: per-board)")
    args = ap.parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    board = args.board or (infer_from_dir(args.dir) if args.dir else None)
    if board is None:
        info = detect_boards.probe_ports(
            sorted(glob.glob("/dev/cu.usbserial*")),
            detect_boards.PROBE_S, want_ip=False)
        found = [d.get("board") for d in info.values()]
        uniq = sorted({b for b in found if b})
        if len(uniq) == 1:
            board = uniq[0]
        else:
            sys.exit("boards found: %r - pass --board" % found)

    cfg = BOARDS[board]
    bindir = os.path.abspath(args.dir or os.path.join(here, "..", cfg["dir"]))
    if not os.path.isfile(os.path.join(bindir, "cyd-dashboard.ino.bin")):
        sys.exit("no image in %s (compile first - see DEVELOPER.md)" % bindir)
    speed = args.speed or cfg["speed"]

    port = args.port or detect_boards.find_board_port(board)
    if not port:
        sys.exit("no port reported board=%s - pass --port, or the firmware "
                 "predates the board marker" % board)
    print("flashing %s -> %s @ %d baud" % (bindir, port, speed))

    cmd = ["arduino-cli", "upload", "-b", FQBN,
           "--input-dir", bindir, "-p", port,
           "--upload-property", "upload.speed=%d" % speed]
    print("+ " + " ".join(cmd))
    sys.exit(subprocess.call(cmd))


def infer_from_dir(d):
    base = os.path.basename(os.path.normpath(d)).lower()
    if base == "release":          # the documented 2.8" build dir
        return "2432s028r"
    for b in BOARDS:
        if b in base:
            return b
    return None


if __name__ == "__main__":
    main()
