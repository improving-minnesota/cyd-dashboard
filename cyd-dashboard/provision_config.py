#!/usr/bin/env python3
"""Provision credentials to the cyd-dashboard directly into NVS over serial.

Reads the git-ignored `.env` file next to this script and streams the credential
keys to the device's USB serial port. The firmware's `serialProvision()` (in
wifi_config.ino) listens for a few seconds at boot and writes each KEY=VALUE line
straight into NVS via Preferences.

The script resets the board, waits for the firmware to announce its boot
provisioning window (`PROV: listen`), then sends the keys and confirms the
`=OK` acknowledgements. It never touches the filesystem, so existing files
(e.g. /pool.csv pool temp history) are preserved, and nothing is compiled into the
firmware.

Usage:
    .venv/bin/python provision_config.py [--port /dev/cu.usbserial-XXXX]
"""

import argparse
import os
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is required. Install with: .venv/bin/pip install pyserial")

CRED_KEYS = [
    "WIFI_SSID",
    "WIFI_PASSWORD",
    "OPENSKY_CLIENT_ID",
    "OPENSKY_CLIENT_SECRET",
    "GOVEE_KEY",
    "IGNORE_AIRPORT",
]


def load_env(path):
    creds = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, val = line.partition("=")
            key = key.strip()
            val = val.strip()
            if key in CRED_KEYS and val:
                creds[key] = val
    return creds


def reset_board(ser):
    """Classic CP2102/ESP32 reset: pulse EN (RTS) low while GPIO0 (DTR) high,
    then release, to reset into normal run mode (not the bootloader)."""
    ser.setDTR(False)
    ser.setRTS(True)          # RTS low -> EN pulled low -> reset held
    time.sleep(0.1)
    ser.setRTS(False)         # release EN -> boots
    time.sleep(0.1)
    ser.setDTR(True)
    ser.reset_input_buffer()


def wait_for_marker(ser, marker, timeout=6.0):
    """Read lines until one contains `marker`; return the matched line or None."""
    end = time.time() + timeout
    buf = b""
    while time.time() < end:
        data = ser.read(ser.in_waiting or 1)
        if data:
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line = line.decode("utf-8", "replace").strip()
                if marker in line:
                    return line
        else:
            time.sleep(0.05)
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", default="/dev/cu.usbserial-1140")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    env_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env")
    if not os.path.exists(env_path):
        sys.exit(f".env not found at {env_path}")
    creds = load_env(env_path)
    if not creds:
        sys.exit("no recognized credential keys found in .env")

    print("Credential keys to provision:")
    for k in creds:
        print(f"  {k} = {'*' * len(creds[k])}")

    ser = serial.Serial(args.port, args.baud, timeout=0.1)
    try:
        reset_board(ser)
        ann = wait_for_marker(ser, "PROV: listen")
        if not ann:
            sys.exit("timed out waiting for the boot provisioning window")
        print(f"window open: {ann}")

        received = 0
        for k in CRED_KEYS:
            if k in creds:
                ser.write(f"{k}={creds[k]}\n".encode())
                ser.flush()
                ack = wait_for_marker(ser, f"PROV: {k}=", timeout=3.0)
                if ack and "=OK" in ack:
                    print(f"  {k} -> OK")
                    received += 1
                else:
                    print(f"  {k} -> no ack")
        print(f"provisioned {received} key(s) into NVS. Board continues to boot normally.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
