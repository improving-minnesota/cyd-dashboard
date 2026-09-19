#!/usr/bin/env python3
"""Identify which CYD board variant is on each serial port.

Opens every /dev/cu.usbserial-* port, pulses RTS to reboot the boards, and
reads the boot log for "[boot] board=<model>" (firmware prints it on every
boot, any build type) plus "[boot] version=/build=" and "[net] ip=" when they
appear. Prints a table of port -> board; --json for machine consumption.

Firmware predating the board marker reports board=unknown - fall back to the
printed IP/version or pass an explicit --port to the update scripts.

Usage (from the repo root):
  cyd-horizon/.venv/bin/python scripts/detect_boards.py [--json]
      [--wait-ip] [--timeout SECS]
"""

import argparse
import glob
import json
import re
import sys
import time

import serial

BAUD = 115200
PROBE_S = 12        # board=/version= print within ~1s of reset
PROBE_IP_S = 30     # [net] ip= needs a WiFi association; allow longer


def probe_ports(ports, timeout, want_ip):
    """Reset every port's board, then collect boot output in parallel.

    Returns {port: {"board": str, "version": str, "build": str, "ip": str}} -
    keys absent when the corresponding line never appeared.
    """
    sers = {}
    info = {p: {} for p in ports}
    bufs = {p: b"" for p in ports}
    try:
        for p in ports:
            try:
                sers[p] = serial.Serial(p, BAUD, timeout=0)
            except serial.SerialException as e:
                print("  %s: cannot open (%s)" % (p, e), file=sys.stderr)
        # Pulse RTS on all of them so they boot together.
        for s in sers.values():
            s.dtr = False
            s.rts = True
        time.sleep(0.1)
        for s in sers.values():
            s.rts = False

        deadline = time.time() + timeout
        while time.time() < deadline:
            for p, ser in sers.items():
                chunk = ser.read(4096)
                if chunk:
                    bufs[p] += chunk
            # Early exit once every port has reported a board marker; when
            # --wait-ip, hold out for [net] ip= too.
            if not want_ip and all("board" in info[p] for p in sers):
                break
            if want_ip and all("board" in info[p] and "ip" in info[p]
                               for p in sers):
                break
            # Parse complete lines so far.
            for p in sers:
                while b"\n" in bufs[p]:
                    line, bufs[p] = bufs[p].split(b"\n", 1)
                    line = line.decode("utf-8", "replace").strip()
                    m = re.search(r"\[boot\] board=(\S+)", line)
                    if m:
                        info[p]["board"] = m.group(1)
                    m = re.search(r"\[boot\] version=(\S+) build=(\d+)", line)
                    if m:
                        info[p]["version"], info[p]["build"] = m.groups()
                    m = re.search(r"\[net\] ip=(\S+)", line)
                    if m:
                        info[p]["ip"] = m.group(1)
            time.sleep(0.05)
    finally:
        for s in sers.values():
            s.close()
    return info


def find_board_port(board, ports=None, timeout=PROBE_S):
    """Return the serial port whose boot log reports `board`, or None."""
    if ports is None:
        ports = sorted(glob.glob("/dev/cu.usbserial*"))
    info = probe_ports(ports, timeout, want_ip=False)
    for p, d in info.items():
        if d.get("board") == board:
            return p
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--json", action="store_true", help="print JSON mapping")
    ap.add_argument("--wait-ip", action="store_true",
                    help="keep listening until each board prints [net] ip=")
    ap.add_argument("--timeout", type=int, default=None)
    args = ap.parse_args()

    ports = sorted(glob.glob("/dev/cu.usbserial*"))
    if not ports:
        sys.exit("no /dev/cu.usbserial-* ports found - is a board plugged in?")
    timeout = args.timeout or (PROBE_IP_S if args.wait_ip else PROBE_S)
    info = probe_ports(ports, timeout, args.wait_ip)

    if args.json:
        print(json.dumps(info, indent=2))
        return
    for p in ports:
        d = info.get(p, {})
        desc = d.get("board", "unknown")
        extra = []
        if "version" in d:
            extra.append("v%s b%s" % (d["version"], d.get("build", "?")))
        if "ip" in d:
            extra.append(d["ip"])
        print("%-28s %s%s" % (p, desc,
                              ("  " + " ".join(extra)) if extra else ""))


if __name__ == "__main__":
    main()
