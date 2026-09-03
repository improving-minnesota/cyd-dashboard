#!/usr/bin/env python3
"""Provision the airline logos onto the dedicated LittleFS "logos" partition.

This is a one-time-per-device step (re-run any time you add/remove/update a
logo). It does NOT touch the app slots, so OTA updates afterwards keep the
logos already on the device. Steps:

1. Convert the local source PNGs into <ICAO>.bin logo files (convert_logos.py
   --out-dir), unless --skip-convert is given.
2. Pack them into a LittleFS filesystem image sized to the logos partition
   (512KB by default) using the mklittlefs tool shipped with the ESP32 core.
3. Write the image to the logos partition via esptool (skip if --no-flash).

Usage:
    .venv/bin/python provision_logos.py [--port /dev/cu.usbserial-XXXX]
    .venv/bin/python provision_logos.py --no-flash   # only build the image
"""
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ARDUINO15 = os.path.expanduser("~/Library/Arduino15/packages/esp32")

# Defaults match the "logos" partition in partitions.csv (0x370000, 512KB).
DEFAULT_OFFSET = 0x370000
DEFAULT_SIZE = 512 * 1024


def find_tool(rel, name, override):
    """Return an explicit --override path, or locate the newest version in
    ~/Library/Arduino15/packages/esp32/tools/<rel>."""
    if override:
        return override
    base = os.path.join(ARDUINO15, "tools", rel)
    if not os.path.isdir(base):
        sys.exit(f"tool directory not found: {base}")
    versions = sorted(os.listdir(base))
    if not versions:
        sys.exit(f"no tools found under {base}")
    return os.path.join(base, versions[-1], name)


def run(cmd):
    print("+", " ".join(shlex_quote(c) for c in cmd))
    subprocess.run(cmd, check=True)


def shlex_quote(s):
    return "'" + str(s).replace("'", "'\\''") + "'"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", help="serial port to flash to (omit to skip flash)")
    ap.add_argument("--baud", type=int, default=460800)
    ap.add_argument("--offset", default="0x370000",
                    help="flash offset of the logos partition")
    ap.add_argument("--size", type=int, default=DEFAULT_SIZE,
                    help="logos partition size in bytes")
    ap.add_argument("--out-dir", default=os.path.join(HERE, "build", "logos"),
                    help="where convert_logos.py writes the <ICAO>.bin files")
    ap.add_argument("--image", default=os.path.join(HERE, "build", "logos.img"),
                    help="LittleFS image file to create")
    ap.add_argument("--skip-convert", action="store_true",
                    help="reuse existing --out-dir instead of regenerating")
    ap.add_argument("--no-flash", action="store_true",
                    help="only build the image, do not write to the device")
    ap.add_argument("--mklittlefs", help="path to mklittlefs (overrides auto-locate)")
    ap.add_argument("--esptool", help="path to esptool (overrides auto-locate)")
    args = ap.parse_args()

    # 1. Convert PNGs -> .bin files.
    if not args.skip_convert:
        convert = os.path.join(HERE, "convert_logos.py")
        run([sys.executable, convert, "--out-dir", args.out_dir])
    if not os.path.isdir(args.out_dir) or not any(
            f.endswith(".bin") for f in os.listdir(args.out_dir)):
        sys.exit(f"no logo .bin files found in {args.out_dir}")

    # 2. Pack into a LittleFS image.
    mklittlefs = find_tool("mklittlefs", "mklittlefs", args.mklittlefs)
    os.makedirs(os.path.dirname(args.image), exist_ok=True)
    run([mklittlefs, "-c", args.out_dir,
         "-b", "4096", "-p", "256",
         "-s", str(args.size), args.image])
    print(f"\nWrote LittleFS image ({os.path.getsize(args.image)} bytes) "
          f"to {args.image}")

    # 3. Flash to the logos partition.
    if args.no_flash or not args.port:
        print("Not flashing (--no-flash or no --port).")
        return 0
    esptool = find_tool("esptool_py", "esptool", args.esptool)
    run([esptool, "--chip", "esp32", "--port", args.port,
         "--baud", str(args.baud),
         "write_flash", args.offset, args.image])
    print("\nLogos flashed to the 'logos' partition. "
          "The device keeps them across OTA updates.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
