# cyd-dashboard

[![License: MPL 2.0](https://img.shields.io/badge/License-MPL_2.0-brightgreen.svg)](LICENSE)
[![PR - Lint & Build](https://github.com/improving-minnesota/cyd-dashboard/actions/workflows/pr.yml/badge.svg)](https://github.com/improving-minnesota/cyd-dashboard/actions/workflows/pr.yml)
[![Release](https://img.shields.io/github/v/release/improving-minnesota/cyd-dashboard)](https://github.com/improving-minnesota/cyd-dashboard/releases)

A self-contained, AI Vibe-coded, **ESP32 touchscreen dashboard** for the **Cheap Yellow Display
(CYD)** — the common ESP32-2432S028R board with a built-in 2.8" 320x240
touchscreen. It turns that little display into a live weather station, flight
tracker, and Govee pool temp monitor. Once it's set up it runs on its own over your WiFi —
no computer needed.

## What it does

- **Clock** — a big time and date in the header.
- **Weather** — current temperature, "feels like", humidity, sunrise/sunset, and
  a 7-day forecast, refreshed every 10 minutes.
- **Flight tracker** — live aircraft overhead (from OpenSky) with a mini radar,
  callsign, altitude/speed/distance, origin & destination, and airline logo. The
  LED flashes red or green when a flight is departing or arriving at DFW, and
  blue for flights to/from a top US airport.
- **Govee pool temp monitor** — current pool water temperature from a Govee
  thermometer, with a history graph (Day / Week / Month / Year) showing the
  low, average, and high.
- **Sleep mode** — deep-sleeps overnight and wakes on touch.
- **Firmware updates** — updates itself over WiFi from GitHub, either
  automatically once a day or manually from the About screen.

## Hardware

This firmware is built for the **Cheap Yellow Display (CYD)** — specifically the
**ESP32-2432S028R** board. It combines an ESP32 microcontroller, a 2.8"
(320x240) color display, and a resistive touchscreen in a single ready-to-run
unit, so no separate wiring or external display is needed.

The code is tailored to this board's exact wiring (TFT on HSPI, touch on VSPI,
touch IRQ on GPIO 36), so **it won't work on other ESP32 boards or displays
without modification**. Developers can find the wiring and flashing details in
[DEVELOPER.md](DEVELOPER.md).

## Getting started

1. **Flash a compiled image to the device.** The firmware isn't preinstalled —
   you must upload a compiled image to the board over USB. Building and
   flashing instructions are in [DEVELOPER.md](DEVELOPER.md).
2. **Power it up.** The first boot runs a short setup wizard: it calibrates the
   touchscreen (if no calibration is saved), then walks you through entering
   your WiFi name and password. To change WiFi later, open **Settings → WiFi**.
3. If the wizard is skipped or you need to redo a step: calibrate via
   **Settings → General → Calibrate Touch** (or hold anywhere on the screen for
   10 s), and connect WiFi via **Settings → WiFi**.
4. **Weather and flights work out of the box.** Optionally add your own OpenSky
   credentials to raise the flight-API rate limit and remove the yellow warning regarding anonymous usage.
5. **Govee pool thermometer (optional):** if you have one, add your Govee API
   key on **Settings → Pool Temp**, then tap **Fetch Devices** to select your
   thermometer.
6. **Airline logos (optional):** consider adding airline logos so the flight
   view shows each carrier's icon. See [DEVELOPER.md](DEVELOPER.md) →
   "Airline logos" for how to provision them.

Where to get each credential is explained below.

### Getting the credentials

- **WiFi** — the SSID (network name) and password for your home network. Find
  them on your router, or ask whoever set up your WiFi.
- **OpenSky (optional, for flights)** — flights work out of the box, but you can
  raise the rate limit by making a free account at
  [opensky-network.org](https://opensky-network.org) and creating an API client
  under **My OpenSky → Account** to get a client ID and secret.
- **Govee (optional, for the pool temp monitor)** — create a free developer
  account at [developer.govee.com](https://developer.govee.com) and generate an
  API key.

## Using the device

- Tap the **Settings** cog to open the settings menu.
- On the idle screen, tap the **Pool** reading to open its history graph.
- On the idle screen, tap the **aircraft status** in the lower-left (e.g. "6
  aircraft") to open the last overhead flight's details (its route, speed, etc.).
  If no flight has been seen yet, it shows dashes.
- On any on-screen keyboard, **tap into the text field to place the cursor**, so
  you can insert or delete characters in the middle of a value (handy for
  lat/lon, an address, or a password).
- Every setting is explained in **Settings → Help** on the device, and in the
  user guide below.

## User guide

Defaults for a freshly reset device are shown with each setting.

- **General** — set the **Clock Color** (the dashboard's clock/header bar) and
  toggle **Auto-Update** (whether the device checks for and installs firmware
  updates). Defaults: clock color **blue**, auto-update **on** (for release
  builds).
- **Location** — set your coordinates so weather and flights are accurate. Use
  **Set** to type them, **Search Address**, or **Find by IP**. Default: none —
  on first boot it's guessed from your IP, otherwise your saved location.
  **Search Address** keeps your last search so you can fix it, and shows a clear
  message if the address can't be found.
- **WiFi** — your network, plus any API credentials (OpenSky, Govee). Default:
  none — you must enter your WiFi.
- **Flight Tracker** — on/off, units (mi or km), radar radius, altitude ceiling,
  poll interval, the countdown/timer bar on the dashboard, your home airport,
  and whether to blink the LED when a noteworthy flight is overhead. Also where
  you enter your OpenSky credentials. Defaults: **on**, imperial units,
  **3.5 mi** radius, **15,000 ft** ceiling, **60 s** poll, timer **off**, no
  home airport, blinking **on**.
- **Sleep Mode** — enable it, set the start/end time, and the wake duration.
  Defaults: **on**, sleeps 10:00 PM – 8:00 AM, **10 min** wake.
- **Pool Temp** — enable it, enter your Govee API key, and pick your
  thermometer. Default: **off**.
- **Calibrate Touch** — recalibrate the touchscreen if taps land in the wrong
  spot.
- **About** — version, author, and firmware update status. When a newer version
  is available it shows **Upgrade Available** with an **Install** button.
- **Help** — this guide, on the device.
- **Reset** — confirms before wiping and shows a message saying exactly what's
  being reset. **All** clears settings *and* all files (including pool
  temperature history) and touch calibration, so the next boot asks you to
  recalibrate; **Settings** clears settings and credentials only but keeps
  calibration. Both reboot the device. **Cancel** changes nothing.

## Updates (OTA)

This firmware can update itself over WiFi from this repository's GitHub
releases, so you don't need a computer to install new versions.

- **Auto-Update** (General, default **on** for release builds) checks once a
  day — after the device boots, connects to WiFi and syncs the clock — and
  installs a newer version if one exists. It won't run more than once per day.
  Turning Auto-Update on after it was off clears the last-check date so it can
  check again the same day.
- **Manually** — open **Settings → About**; it checks for a newer version and
  shows **Upgrade Available (vX.Y.Z)** with an **Install** button. Tap it to
  update right away.
- During an update the screen shows progress and **"Do not power off device"**.
  When it finishes, the device restarts into the new firmware.

If the new firmware fails to start, the device automatically rolls back to the
previous version.

## Troubleshooting

**The screen isn't accurate when you touch it.** The touchscreen can drift over
time or after a firmware update. To recalibrate, press and hold anywhere on the
screen for **10 seconds** — a calibration screen appears and walks you through
tapping the target dots. When you're done, touch accuracy is restored.

**What does the colored border around the screen mean?** A **red** border flags a
critical issue (no WiFi, bad OpenSky credentials, exhausted flight credits, or
pool data unavailable). A **yellow** border means you're using OpenSky
**anonymously** — not an error, flights still work, just at a lower rate limit.


## License

This project is licensed under the [Mozilla Public License 2.0](LICENSE).

---

For developers (flashing the firmware, hardware/TFT setup, and provisioning
credentials), see [DEVELOPER.md](DEVELOPER.md).
