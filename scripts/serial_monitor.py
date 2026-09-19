#!/usr/bin/env python3
"""Passive serial monitor for the CYD - a friendlier `screen`/`cu` for log
watching during development.

pyserial does not reset this board's CH340 on open, so this attaches mid-run
without losing the device's state (unlike the ota_push.py open, which pulses
RTS deliberately). Pass --reset for a clean boot log from POWERON_RESET.
Ctrl-C exits; no screen-style escape chords needed.

Usage (from the repo root):
  cyd-horizon/.venv/bin/python scripts/serial_monitor.py \
      [--port /dev/cu.usbserial-XXXX] [--baud 115200] [--reset]
"""

import argparse
import glob
import sys
import time

import serial


def find_port(cli_port):
    if cli_port:
        return cli_port
    ports = sorted(glob.glob("/dev/cu.usbserial*"))
    if len(ports) == 1:
        return ports[0]
    sys.exit("serial port ambiguous/missing; found %r - pass --port" % ports)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="serial port (default: sole /dev/cu.usbserial*)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--reset", action="store_true",
                    help="pulse RTS on open so the log starts at POWERON_RESET")
    args = ap.parse_args()

    port = find_port(args.port)
    ser = serial.Serial(port, args.baud, timeout=0.25)
    print("monitoring %s @ %d%s - Ctrl-C to quit"
          % (port, args.baud, " (reset)" if args.reset else ""))
    if args.reset:
        # DTR stays released so the board boots normally, not download mode.
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False

    try:
        while True:
            chunk = ser.read(4096)
            if chunk:
                sys.stdout.write(chunk.decode("utf-8", "replace"))
                sys.stdout.flush()
    except KeyboardInterrupt:
        print()


if __name__ == "__main__":
    main()
