# cyd-dashboard

[![License: MPL 2.0](https://img.shields.io/badge/License-MPL_2.0-brightgreen.svg)](LICENSE)
[![PR - Lint & Build](https://github.com/improving-minnesota/cyd-dashboard/actions/workflows/pr.yml/badge.svg)](https://github.com/improving-minnesota/cyd-dashboard/actions/workflows/pr.yml)
[![Release](https://img.shields.io/github/v/release/improving-minnesota/cyd-dashboard)](https://github.com/improving-minnesota/cyd-dashboard/releases)

A self-contained, AI Vibe-coded, **ESP32 touchscreen dashboard** for **Cheap
Yellow Display (CYD)-family boards** — ESP32 boards with a built-in color
touchscreen, like the 2.8" 2432S028R and the 4" E32R40T. It turns that little display into a live weather station, flight
tracker, and Govee pool temp monitor. Once it's set up it runs on its own over your WiFi —
no computer needed.

> **Giving a device to a non-techie friend?** Grab the printable two-page
> [User Guide (PDF)](docs/user-guide/DEVELOPER-USERGUIDE.pdf) — written for
> non-technical users; front page is setup, back page is everyday reference.

<table>
  <tr>
    <td><img src="docs/user-guide/DEVELOPER-USERGUIDE-dashboard.png" width="150" alt="Flight-overhead dashboard view"></td>
    <td><img src="docs/user-guide/DEVELOPER-USERGUIDE-idle.png" width="150" alt="Idle weather and pool view"></td>
    <td><img src="docs/user-guide/DEVELOPER-USERGUIDE-wxgraph.png" width="150" alt="Weather temperature history graph"></td>
    <td><img src="docs/user-guide/DEVELOPER-USERGUIDE-ftracker.png" width="150" alt="Flight Tracker settings"></td>
  </tr>
</table>

## What it does

- **Clock** — a big time and date in the header, with a small AM/PM marker.
- **Weather** — current temperature, "feels like", humidity, sunrise/sunset, and
  a 7-day forecast, refreshed every 10 minutes. The current temperature is also
  logged to flash (always on) and graphed over Day / Week / Month / Year with the
  low, average, and high.
- **Flight tracker** *(optional; on by default)* — live aircraft overhead
  (from OpenSky) with a mini radar,
  callsign, altitude/speed/distance, planned route from ADSB.lol shown as
  `ICAO | IATA` airport codes when IATA is available (with OpenSky actual-route
  fallback; toggleable in Flight Tracker settings), airline
  logo, and a dotted ground-track line showing
  where the plane actually came from. The LED blinks blue for every overhead
  flight, red when departing your home airport (ICAO), green when arriving at
  your home airport (ICAO), yellow when both origin and destination are your
  home airport, or white for a watched callsign while its flight details are
  shown. Red, green, and yellow stay lit while the live flight is displayed;
  white repeats while the watched callsign is live. On the home screen the LED
  also glows red for a critical error or yellow when OpenSky is anonymous,
  matching the on-screen border. Critical errors include No WiFi, invalid
  OpenSky credentials, exhausted OpenSky credits, unavailable OpenSky/weather
  data, and an unavailable Govee pool temp.
- **Govee pool temp monitor** *(optional; off by default)* — current pool
  water temperature from a Govee thermometer, with a history graph
  (Day / Week / Month / Year) showing the low, average, and high.
- **Sleep mode** — deep-sleeps overnight and wakes on touch.
- **Alarms** — up to 6 time-of-day alarms, each with its own days-of-week mask
  and notification pattern (Blink, Rapid, Double, Colors, Pulse — each plays a
  matching LED + speaker alert). Sound requires an **external speaker**
  plugged into the board's JST speaker header (GPIO 26) — the CYD has no
  built-in speaker, so without one alarms flash the LED only. When one
  fires, big **Dismiss** and **Snooze** (5 min) buttons appear; up to 3
  snoozes, then it dismisses itself for the day. A small
  bell icon appears next to the header clock while any alarm is enabled.
  Alarms fire on time even during Sleep Mode — the device wakes at the alarm
  time — and a snoozed or missed alarm still fires after a power loss.
- **Firmware updates** — updates itself over WiFi from GitHub, either
  automatically once a day or manually from the About screen.

## Hardware

This firmware is built for the **Cheap Yellow Display (CYD)** family. Two
boards are supported:

- **ESP32-2432S028R** — the common 2.8" (320x240) board.
- **E32R40T** — the 4.0" (480x320) variant; the same UI is scaled up with
  smoother fonts.

Each combines an ESP32, a color display, and a resistive touchscreen in a
single ready-to-run unit, so no separate wiring or external display is needed.

The code is tailored to these boards' exact wiring, so **it won't work on
other ESP32 boards or displays without modification** — new variants get their
own `-DCYD_<MODEL>` build flag (see [DEVELOPER.md](DEVELOPER.md) → "Board
variants"). Developers can find the wiring and flashing details in
[DEVELOPER.md](DEVELOPER.md).

## Getting started

1. **Flash a compiled image to the device.** The firmware isn't preinstalled —
   you must upload a compiled image to the board over USB. Building and
   flashing instructions are in [DEVELOPER.md](DEVELOPER.md).
2. **Power it up.** The first boot runs a short setup wizard: it calibrates the
   touchscreen (if no calibration is saved), then walks you through entering
   your WiFi name and password. To change WiFi later, open **Settings →
   Network**.
3. If the wizard is skipped or you need to redo a step: calibrate via
   **Settings → General → Calibrate Touch** (or hold anywhere on the screen for
   10 s), and connect WiFi via **Settings → Network**.
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
  under **My OpenSky → Account** — that downloads a file containing your
  **client ID** and **client secret**.
- **Govee (optional, for the pool temp monitor)** — create a free developer
  account at [developer.govee.com](https://developer.govee.com) and generate an
  API key.

## Using the device

- Tap the **Settings** cog to open the settings menu.
- Tap the **clock/date** in the header to open **Alarms**. Set the time with
  the **▼**/**▲** arrows beside it — hour on the left, minute on the right
  (the hour wraps through AM/PM on its own) — or tap the time itself to type
  it, toggle the weekdays it applies to, and pick a notification pattern. Use
  **New**/**<**/**>** to manage multiple alarms; **Del** asks for
  confirmation before removing one.
- With **Flight Tracker** on, the header shows your remaining **OpenSky
  credits** as three small readouts — **CRP** (radar polling), **CRL** (route
  lookup), and **CFT** (flight tracking). A value is **grey** when healthy,
  turns **yellow** below 500 (or shows a yellow **?** until that bucket's first
  fetch), and **pink** below 50. A `429` rate/credit-limited response for any
  bucket sets that path to 0. Tap the readouts to open the **OpenSky Credits**
  screen.
- On the idle screen, tap the **weather temperature** (top-left) to open the
  weather temperature history graph.
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

- **General** — set the **Clock Color** (the dashboard's clock/header bar) on
  a swatch picker (tap a hue, then a shade or grey), toggle **Auto-Update** (whether the device
  checks for and installs firmware updates), pick the **Units**
  (**Imperial** ft/mph/mi, **Metric** m/kts/km, or **Aviation** ft/kts/nm —
  also sets weather and pool temps to °F or °C), and choose a **12 or
  24-hour clock** (24-hour hides the AM/PM marker). The picked color also
  themes the
  ordinary screen buttons (black text on light colors, white on dark).
  Defaults: color **blue**, auto-update **on** (for release builds), units
  **imperial**, clock **12-hour**.
- **Location** — set your coordinates so weather and flights are accurate. Use
  **Set** to type them, **Search Address**, or **Find by IP**. Default: none —
  on first boot it's guessed from your IP, otherwise your saved location.
  **Search Address** keeps your last search so you can fix it, and shows a clear
  message if the address can't be found.
- **Network** — your WiFi network (it scans in the background — pick from the
  list once "Scanning" finishes, or enter it manually), plus **IP
  setup**: addressing can stay on **DHCP** (the default, works as before) or be
  switched to **Static** with an IP address, subnet mask, gateway, and DNS
  server, and you can set a device **hostname** (default `cyd-dashboard`) in
  either mode. A blank DNS
  uses the gateway; incomplete static fields fall back to DHCP. Changes apply
  on the next connect.
- **Flight Tracker** — on/off, radar radius (how far away to look,
  shown in mi/km/nm), altitude ceiling (planes above it are ignored, shown in
  ft/m — radius and ceiling follow the Units setting under General, and
  switching units keeps the same number and re-reads it in the new
  unit), poll interval, the countdown/timer bar on the dashboard, your home
  airport (ICAO),
  a watched callsign (blinks white while its flight details are shown), and
  whether to blink the LED for an overhead flight. Also where you enter your
  OpenSky credentials. The LED blinks blue for every overhead flight, yellow when
  both origin and destination are your home airport (ICAO), green when arriving,
  red when departing, or white for a watched callsign. Red, green, and yellow stay
  lit while the live flight is displayed; white repeats while the watched
  callsign is live. On the home screen the LED also glows red for a critical
  error or yellow when OpenSky is anonymous, matching the on-screen border.
  Critical errors include No WiFi, invalid OpenSky credentials, exhausted
  OpenSky credits, unavailable OpenSky/weather data, and an unavailable Govee
  pool temp. Any
  settings or graph screen automatically returns to the main dashboard after 2
  minutes of inactivity.
  Defaults: **on**,
  **3.5 mi** radius, **15,000 ft** ceiling, **30 s** poll, timer **off**, no home
  airport set, no watched callsign, blinking **on**. If your OpenSky **radar-polling** credits run
  out, the poll backs off to a slower 15-minute recovery check until they refill
  (OpenSky resets daily).
- **Sleep Mode** — enable it, set the start/end time, and the wake duration.
  Defaults: **on**, sleeps 10:00 PM – 8:00 AM, **5 min** wake.
- **Pool Temp** — enable it, enter your Govee API key, and pick your
  thermometer. Default: **off**.
- **Calibrate Touch** — recalibrate the touchscreen if taps land in the wrong
  spot.
- **About** — version, author, and firmware update status. When a newer version
  is available it shows **Upgrade Available** with an **Install** button.
- **Help** — this guide, on the device.
- **Reset** — confirms before wiping and shows a message saying exactly what's
  being reset. **Factory Reset** clears settings *and* all files (including
  pool and weather temperature history and airline logos) and touch
  calibration, so the next boot asks you to recalibrate — ⚠️ if the device
  came pre-loaded with airline logos, restoring logos requires regenerating
  and reloading them from a development computer; **Settings** clears settings and
  credentials; **Graph Data**
  clears pool and weather temperature history. All three reboot the device.
  **Restart** reboots without clearing anything; **Cancel** changes nothing.

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
critical issue (no WiFi, bad OpenSky credentials, exhausted OpenSky
radar-polling credits, or pool data unavailable). A **yellow** border means
you're using OpenSky **anonymously** — not an error, flights still work, just at
a lower rate limit.


## License

This project is licensed under the [Mozilla Public License 2.0](LICENSE).

---

For developers (flashing the firmware, hardware/TFT setup, and provisioning
credentials), see [DEVELOPER.md](DEVELOPER.md).
