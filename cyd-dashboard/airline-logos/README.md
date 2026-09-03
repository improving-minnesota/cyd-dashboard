# Airline logo source images

This directory holds the airline logo source images (PNG icons) that
`convert_logos.py` converts into per-airline `<ICAO>.bin` files, which are then
provisioned onto the device's dedicated LittleFS **logos** partition.

**These source images are git-ignored** — they are kept locally and not
committed, because we're not sure we can redistribute the brand logos. The
logos are **not** compiled into the firmware: they are loaded at runtime from
the logos partition (see `DEVELOPER.md` → "Partition table" and "Airline
logos"). A device with an empty logos partition simply draws no airline logos.

To regenerate the logo files and flash them to the device after adding or
changing a source image, run:

```bash
cyd-dashboard/.venv/bin/python cyd-dashboard/provision_logos.py --port /dev/cu.usbserial-XXXX
```

Add `--no-flash` to only write the `<ICAO>.bin` files + build the LittleFS
image without touching the device.

See `DEVELOPER.md` → "Airline logos" for the full workflow.
