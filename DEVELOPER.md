# Developer / Advanced Details

Advanced setup for the `cyd-dashboard` firmware. For an end-user overview and
the settings guide, see the [README](README.md).

## Author

- **Site:** improving.com
- **Developer:** Paul Hassinger
- **Email:** paul.hassinger (at) improving.com

## Development workflow

All changes to this repo are made through **pull requests** that are merged
into `main`. Never commit directly to `main`. The workflow is:

1. **Create a feature branch** off the latest `main`:
   ```bash
   git checkout main
   git pull
   git checkout -b feature/short-description
   ```
2. **Make your changes** and commit them on the branch, following the project's
   existing commit style (short imperative subject lines).
3. **Push the branch** to GitHub:
   ```bash
   git push -u origin feature/short-description
   ```
4. **Open a pull request** against `main` (e.g. `gh pr create` or via the
   GitHub UI), including a summary of the change and how it was verified
   (compile/upload results, screenshots of the display, etc.).
5. **Review and address feedback**, keeping the branch up to date with `main`
   as needed (rebase or merge, then re-push).
6. **Merge the PR into `main`** once it's reviewed and green. Delete the branch
   locally and on the remote after merging:
   ```bash
   git branch -d feature/short-description
   git push origin --delete feature/short-description
   ```

Keep PRs small and focused on a single logical change so they're quick to
review. The primary branch is `main`; everything that lands there is intended
to be shippable.

## The sketch

The `cyd-dashboard/` sketch targets the **ESP32-2432S028R "CYD"**
(`esp32:esp32:jczn_2432s028r`). It uses a custom partition table (see
"Partition table" below), so the FQBN must include `:PartitionScheme=custom`:

```bash
arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:PartitionScheme=custom cyd-dashboard
arduino-cli upload -p /dev/cu.usbserial-XXXX -b esp32:esp32:jczn_2432s028r:PartitionScheme=custom --upload-property upload.speed=115200 cyd-dashboard
```

> **Important:** the display pinout is configured in the sketch's own
> `cyd-dashboard/tft_setup.h`. TFT_eSPI auto-detects a `tft_setup.h` in the
> sketch folder and uses it instead of its `User_Setup_Select.h`, so **no edits
> to the installed TFT_eSPI library are needed** and the build is identical
> locally and in CI. It sets the correct CYD wiring (TFT on HSPI, touch on VSPI,
> touch IRQ GPIO 36); touch-wake from deep sleep uses GPIO 36.

## Continuous integration & releases

GitHub Actions builds and releases the firmware on standard hosted runners
(these are free/unlimited for **public** repos). Two workflows live in
`.github/workflows/`:

- **`pr.yml`** — runs on every PR to `main`:
  1. **Lint** the PR title with `amannn/action-semantic-pull-request` (conventional
     commits). This is **required** for release-please to determine the next
     version in `release.yml`, so PR titles must look like `feat: Add widget`,
     `fix: Correct scaling`, `chore: Update docs`, etc.
  2. **Build** the firmware: installs arduino-cli, the `esp32` core (3.3.11),
     TFT_eSPI (2.5.43) and ArduinoJson (7.4.3), then compiles with the
     production FQBN. (The CYD display pinout comes from the sketch's own
     `tft_setup.h`, so no library patching is required.) This verifies every
     PR compiles.

> **Docs-only PRs:** `pr.yml` has no path filter, so even a docs-only PR runs
> the (harmless) firmware build. That's intentional — it keeps the "Build
> firmware" check predictable for branch-protection rulesets. For occasional
> one-off doc updates, just let the build run.

- **`release.yml`** — runs on every push/merge to `main` and drives the release
  flow with `googleapis/release-please-action@v4` (`release-type: simple`):
  1. release-please opens a **"release-please" PR** that bumps the version in
     `version.txt` (the `simple` release type reads the current version from
     `version.txt`) and updates `CHANGELOG.md` based on the conventional-commit
     PRs merged since the last release. Merge that PR through the normal review
     process.
  2. Once merged, release-please creates a git tag + GitHub **release**.
  3. A build job then compiles the OTA firmware with
     `-DAPP_VERSION=<version>` (so **Settings → About** shows the release version)
     and uploads `cyd-dashboard.ino.bin` (the raw app image for the inactive OTA
     slot) as a release asset.

The version shown on the About screen comes from the `APP_VERSION` compile-time
macro (`kVersion` in `cyd-dashboard.ino`); it defaults to the current branch
version with a `-dev` suffix (e.g. `1.1.0-dev`) when not set, so local builds
work without the flag. Any `-dev` version is treated as a **dev build** that
never auto-updates (see below). Keep `version.txt` in sync with that default
when you first adopt this. OTA *delivery* of the `.bin` to a device is handled
on-device — see **OTA updates (firmware delivery)** below.

> **Token:** `release.yml` authenticates release-please with a dedicated GitHub
> App installation token (minted by `create-github-app-token` from the
> `RELEASE_PLEASE_CLIENT_ID` / `RELEASE_PLEASE_PRIVATE_KEY` secrets) rather than
> the default `GITHUB_TOKEN`. This is what lets the bot open/update the release
> PR and create the release even where the org blocks the default token from
> opening PRs. If release PRs ever stop picking up checks, verify those secrets
> are still set on the repo.

## OTA updates (firmware delivery)

The device updates itself over WiFi from this repository's public GitHub
releases. All of this lives in `cyd-dashboard/ota.ino`.

### How it works

1. **Check.** `fetchLatestRelease()` GETs
   `https://api.github.com/repos/improving-minnesota/cyd-dashboard/releases/latest`
   (no auth — the repo is public), parses the `tag_name` (e.g. `v1.0.1`), and
   finds the `cyd-dashboard.ino.bin` asset URL.
2. **Compare.** `compareVersions()` / `isNewerThanRunning()` strip the leading
   `v` and compare semver against the running `kVersion`.
3. **Install.** `performOTA()` downloads the `.bin` in 4 KB chunks, streams them
   to the inactive OTA slot via `Update.write()` (`U_FLASH`), shows a progress
   screen, then `Update.end()` + `ESP.restart()`. On success it never returns.
   It runs on a **dedicated 32 KB-stack task** (`otaTaskEntry`, `g_otaTask` in
   `cyd-dashboard.ino`) because the mbedtls TLS handshake overflows the ~8 KB
   default `loopTask`. The OTA task owns the display, so the main loop yields
   while it runs. It is **not** subscribed to the task watchdog, so there is no
   `esp_task_wdt_reset()` call during the download; the HTTP timeouts bound it.

### What triggers a check

- **Daily auto-scan** (`maybeAutoUpdate()`), run once per boot right after WiFi
  connects and NTP syncs. It runs at most once per calendar day (tracked in NVS
  as the epoch day under `lastscan`). If Auto-Update is ON and a newer release
  exists, it starts the OTA. Toggling **Auto-Update** ON in
  **Settings → General** clears `lastscan` so it can check again the same day.
- **Manual** — opening **Settings → About** triggers a check (via the net task,
  so the UI doesn't freeze); if newer, it shows **Upgrade Available (vX.Y.Z)**
  with an **Install** button.

### Auto-Update toggle & dev builds

- `g_autoUpdate` defaults to `true` (persisted as `autoupd`).
- A **dev build** (version ending in `-dev`) actively forces it OFF at boot, even if
  a previous release build left it ON — so flashing source never silently
  upgrades. Dev builds can still update manually from About or by toggling
  Auto-Update on.

### TLS

Both the release-metadata API call and the firmware download are verified against
a minimal root-CA bundle (`kGithubRootCAs` in `ota.ino`): **USERTrust ECC**
(covers the Sectigo chain for github.com/api.github.com) and **ISRG Root X1**
(covers Let's Encrypt for objects.githubusercontent.com). `OTA_CA_EXPIRY` is the
earliest root expiry; past that the client falls back to `setInsecure(true)`.
There is **no** insecure retry on a failed handshake — that would let a
man-in-the-middle defeat certificate validation — so the only insecure path is
the time-gated one (roots expired). Firmware integrity is independently pinned:
`performOTA()` hashes the streamed image (SHA-256) and compares it to the asset's
`digest` from the GitHub API before flashing (an empty digest skips the check).
A mismatch aborts the update without touching the running slot.

### Rollback

After OTA the new slot boots in the ESP32's "pending verify" state. If it
crashes before a grace period, the bootloader rolls back to the previous slot.
`markAppValidBoot()` is called ~30 s into a successful run to cancel that
rollback and lock in the new firmware.

### Partitions

OTA requires two app slots + `otadata`, already present in `partitions.csv`
(`app0`/`app1` at 1.5 MB each, plus `otadata`). The release `.bin` is the **raw
app image** for the inactive slot, built by the release workflow with
`PartitionScheme=custom`.

### Release versioning (release-please)

Versioning is fully automated — you never hand-edit `version.txt` or bump the
version yourself. release-please reads the current version from `version.txt`,
then derives the **next** version from the conventional-commit **PR titles**
merged to `main` since the last release, following Semantic Versioning
(`MAJOR.MINOR.PATCH`):

| PR title | Bump | Example |
|---|---|---|
| `fix: ...` — a bug fix / correction | **PATCH** | `1.2.3 → 1.2.4` |
| `feat: ...` — a new feature or behavior change | **MINOR** | `1.2.3 → 1.3.0` |
| Breaking change — `feat!: ...` / `fix!: ...`, or a commit body with a `BREAKING CHANGE:` footer | **MAJOR** | `1.2.3 → 2.0.0` |
| `docs:`, `chore:`, `refactor:`, `build:`, `ci:` — no functional change | **no release** on its own | — |

How to decide (guidance for this project):

- **PATCH (`fix:`)** — a bug fix or correction that doesn't add functionality.
  Example: `fix: Correct pool graph scaling`.
- **MINOR (`feat:`)** — a new feature or setting, or any change to behavior a
  user would notice. Example: `feat: Add "Blink for Flight" toggle`.
- **MAJOR (breaking)** — a genuinely breaking change, e.g. a partition-table
  change, an incompatible NVS/credentials layout, or anything that requires the
  user to re-provision or re-flash from scratch. Reserve this for real breakage.
  Example: `feat!: Move airline logos to a dedicated partition` (the change that
  reformatted LittleFS).

Behavior notes:

- When a batch of merged PRs contains multiple bump types, the **largest
  applicable bump wins** (breaking > feat > fix).
- A release is only created **after the "release-please" version-bump PR is
  merged** — that automated PR previews exactly which commits are included and
  what the new version will be. Merging it creates the `vMAJOR.MINOR.PATCH` git
  tag + GitHub release, and the build job bakes that version into
  **Settings → About**.
- Non-functional changes (`docs`, `chore`, `refactor`, `build`, `ci`) update the
  changelog but, on their own, do not trigger a release.

## Partition table

`cyd-dashboard/partitions.csv` is a custom partition table (the Arduino
build system picks up a `partitions.csv` in the sketch folder automatically).
It keeps two OTA-capable app slots, trims the app slots from the stock
"default" size, and carves out a dedicated LittleFS **logos** partition so
airline logos are never compiled into the firmware (see "Airline logos"
below):

| Partition | Stock "default" | This project | Notes |
|---|---|---|---|
| app0 / app1 (each) | 1.31 MB | 1.5 MB | Two OTA slots |
| spiffs (LittleFS) | 1.375 MB | 384 KB | Pool temp history |
| logos (LittleFS) | — | 512 KB | Airline logos (runtime) |

384 KB of spiffs is still comfortably above the pool temp history feature's
worst-case usage (the raw/hourly/daily CSV tiers peak at roughly 150-200 KB
combined before compaction trims them), so this isn't expected to be a
practical constraint. The 512 KB logos partition holds ~364 KB of the current
54 logos, leaving ~148 KB of headroom to add more without ever changing the
table again.

Here's the same layout drawn to approximate proportion (bar widths are relative;
exact offsets/sizes are in the table above):

```
0x0000   ┌ HD  bootloader + partition table + NVS (0x9000) + otadata (0xE000)
0x10000  ├─ app0     ██████████████████████████████████████████ 1.5 MB   (OTA slot 0)
0x190000 ├─ app1     ██████████████████████████████████████████ 1.5 MB   (OTA slot 1)
0x310000 ├─ spiffs   ████████████ 384 KB   (pool temp history, "spiffs")
0x370000 ├─ logos    █████████████████ 512 KB   (airline logos, "logos")
0x3F0000 └─ coredump ████ 64 KB
0x400000   (end of 4 MB flash)
```

Key takeaways from the map:

- **app0 / app1** — the two OTA slots. OTA writes the inactive slot and swaps,
  so a failed update rolls back automatically. They hold the logo-less firmware
  (~1.15 MB), leaving ~350 KB of headroom per slot.
- **spiffs** — pool temperature history (LittleFS, global `LittleFS`).
- **logos** — airline logos as files (LittleFS, separate `LogosFS` instance).
  OTA never touches this partition, so logos persist across updates; add/remove/
  update them by re-provisioning files, not reflashing.
- **HD** — system header: bootloader (0x1000), partition table (0x8000), NVS
  settings/credentials (0x9000, survives re-partition since its offset is
  unchanged), and otadata for OTA rollback selection.

Because the FQBN's declared max size only matches reality when
`PartitionScheme=custom` is selected (otherwise the tool still assumes the
stock "default" ceiling and may reject a build that's actually well within the
real partition), **always build/upload with `:PartitionScheme=custom`** as
shown above.

> **One-time consequence when applying this change to an already-provisioned
> board:** NVS (settings/credentials) keeps the same offset, so it survives.
> The LittleFS partitions (pool "spiffs" and "logos") move to different flash
> offsets, so any existing pool temp history data does not carry over — LittleFS
> will format fresh on the next boot — and the logos partition must be
> provisioned once (see "Airline logos"). This is a one-time effect of
> adopting the new partition table, not something that happens on every flash
> afterward. **Once provisioned, do not change the partition table again:**
> moving app0/app1 shifts where OTA updates land, and any table change
> reformats the LittleFS partitions.

### Reducing flash usage

1. **Partition scheme** (see above) - the single biggest lever. Going from the
   stock "default" scheme's 1.31 MB app partition to this project's 1.5 MB
   custom one freed a lot of room.
2. **Keep logos out of the app** - the airline logos (once the largest chunk of
   the binary, ~364 KB for 54 logos) are no longer compiled into the firmware;
   they live in the dedicated LittleFS **logos** partition and are read at
   runtime. The OTA app is therefore logo-less and small. If you add a logo,
   it counts against the 512 KB logos partition's headroom, not the app slots.
   The logo box size (`BOX_W`/`BOX_H` in `convert_logos.py`) still trades
   detail for flash directly (each doubling of width and height quadruples the
   per-logo byte count), which matters for fitting logos in the partition.

When app flash usage becomes tight, the biggest levers are growing the app
slots at the cost of the logos partition, or reducing other data (e.g. the
top-500 airport table in `flight_details.ino`).

## Pool temperature history persistence

The pool-temp history graphs (Day / Week / Month / Year) are backed by a
tiered store on the LittleFS (spiffs) flash partition, so the data survives
reboots and deep-sleep wakes:

- `/pool.csv` — raw `epoch,temp` samples, logged roughly every 5 minutes. Feeds
  the Day & Week graphs. Compacts to keep the newest ~2500 lines (~8.7 days).
- `/pool_hour.csv` — hourly averages. Feeds the Month graph. Compacts to keep
  the newest ~900 lines (~37 days).
- `/pool_day.csv` — daily averages. Feeds the Year graph. Compacts to keep the
  newest ~800 lines (~2.2 years).
- `/pool_rollup.bin` — the in-progress hour/day accumulators, saved right
  before each deep sleep and restored at boot. Each 5-minute deep-sleep wake is
  a fresh boot, so without this the hourly/daily tiers would never flush during
  the night (starving the Month/Year graphs of overnight data).

At boot the CSV tiers are loaded back into RAM ring buffers (keeping the
newest samples) so the graphs can draw them immediately. The graph shows
"Waiting for time sync..." until NTP has synced, so it never plots samples
against an unsynced clock.

On a freshly flashed board the spiffs partition has never been initialized —
sketch-only flashes (bootloader, partition table, app) never write the data
partition — so the first mount would fail. `poolfsInit()` therefore calls
`LittleFS.begin(true)` to format the partition once on that first mount
failure; afterwards it mounts normally. If the partition is moved or erased
(e.g. a partition-table change), it is formatted once again and the previous
history is dropped.

Data collection runs both while awake (a Govee fetch every ~5 min while on the
dashboard) and while asleep (deep-sleep timer wakes every ~5 min that log one
sample and go back to sleep).

### Power loss vs. deep sleep

- **Deep sleep** — flash is retained, so all persisted history survives. On
  each wake the ring buffers are reloaded from the CSV files and the rollup
  accumulator is restored, then the wake logs one more sample before sleeping.
- **Power loss (unplugged)** — LittleFS and NVS are non-volatile, so all history
  and settings survive a full power cycle. Collection simply pauses while there
  is no power and resumes where it left off once powered on; the only "missing"
  data is the samples that would have been logged during the outage itself.
- **Re-flash / partition change** — ordinary firmware re-flashes leave LittleFS
  intact. Replacing the partition table (as when this project adopted the custom
  scheme) moves the LittleFS partition offset, so LittleFS formats fresh and any
  prior history is cleared. That is a one-time event, not something that happens
  on every flash.

## Provisioning credentials

WiFi/OpenSky/Govee credentials are **not compiled into the firmware**. Put them
in the git-ignored `cyd-dashboard/.env` file (`WIFI_SSID`, `WIFI_PASSWORD`,
`OPENSKY_CLIENT_ID`, `OPENSKY_CLIENT_SECRET`, `GOVEE_KEY`), then stream them
into the device's NVS over USB serial:

```bash
cyd-dashboard/.venv/bin/python cyd-dashboard/provision_config.py --port /dev/cu.usbserial-XXXX
```

First-time setup: `python3 -m venv cyd-dashboard/.venv && cyd-dashboard/.venv/bin/pip install pyserial`

### How provisioning works

The firmware's `serialProvision()` (in `wifi_config.ino`) listens for a few
seconds at boot, writes each `KEY=VALUE` line straight into NVS, and the script
confirms the `=OK` acks. To change credentials, edit `.env` and re-run the
script — no firmware re-flash needed.

> **Testing-only code, disabled by default:** the provisioning listener is a
> temporary aid and is compiled **out** by default (`ENABLE_SERIAL_PROVISION`
> is `0` in `cyd-dashboard.ino`), so the serial `.env` flow above only works
> after you set it to `1` and re-flash. Credentials persist in NVS, so once
> provisioned the board works without the listener. To ship production code,
> leave `ENABLE_SERIAL_PROVISION` at `0` (or delete the guarded block in
> `wifi_config.ino` and the `serialProvision()` call in `setup()`).

Credentials themselves (WiFi, OpenSky, Govee) are entered on the device's
settings screens. For instructions on obtaining them, see the README's
[Getting the credentials](#getting-the-credentials) section.

## Airline logos

The flight view shows an airline's logo (when available) plus its name and
brand color. Two separate lookups drive this:

- **Logo bitmap** — loaded at runtime from a dedicated LittleFS **logos**
  partition (see the partition table above). The logos are **not** compiled
  into the firmware.
- **Name & color** — the `kAirlines` table in `flight_details.ino` (this is
  still compiled in; it's just text/color, not bitmap data).

The PNG source icons in `airline-logos/` are **git-ignored** and kept local,
because we're not sure we can redistribute the brand logos. `convert_logos.py`
turns them into per-airline `<ICAO>.bin` files (a 12-byte header + raw RGB565
pixels, stored at their on-screen 72x48 size, transparent color `0xF81F`), and
`provision_logos.py` packs those into a LittleFS image and flashes it to the
logos partition once per device.

At boot, `logosInit()` (in `logos.ino`) mounts the logos partition (an
independent `fs::LittleFSFS` instance on label `"logos"`, separate from the
pool temp history `"spiffs"` one). `findAirlineLogo()` resolves a callsign's 3-letter
ICAO prefix, opens `/<ICAO>.bin`, and returns the bitmap through a small
**bounded LRU cache** (max 16 logos) that allocates buffers **PSRAM-first** when
available, falling back to internal heap. Any failure — partition absent,
file missing, or an allocation failure — simply means **no logo is drawn**; the
device never crashes and OTA firmware never embeds logos.

> **Update cadence:** adding, removing, or replacing a logo is a *filesystem*
> change on the logos partition, not a firmware change. You never touch the
> app slots or recompile for logos, and OTA updates don't affect them.

### Adding a new airline logo

1. **Add a PNG icon** — drop the logo into `cyd-dashboard/airline-logos/`,
   e.g. `FedEx Icon.png`.
2. **Map it in `convert_logos.py`** — add an entry to the `AIRLINES` list:
   `("FedEx", "FDX", "FedEx Express")`. The first item is a keyword matched
   (case-insensitively) against the filename, and must match **exactly one**
   file. If the filename could match another keyword, use a more specific
   keyword and place it **before** the conflicting one — e.g. UPS uses
   `"Parcel"` placed before `"United"` so the UPS icon isn't captured as United
   Airlines.
3. **(Optional) name & color** — if not already present, add the ICAO code,
   display name, and brand color to `kAirlines` in `flight_details.ino`, e.g.
   `{"FDX", "FedEx Express", TFT_PURPLE}`. This drives the name and colored
   badge shown when no logo is available. This one *does* require a recompile.
4. **Generate the logo files and provision them** (no firmware recompile):
   ```bash
   # writes <ICAO>.bin files, packs a LittleFS image, and flashes it to the
   # logos partition. Add --no-flash to only build the image.
   cyd-dashboard/.venv/bin/python cyd-dashboard/provision_logos.py --port /dev/cu.usbserial-XXXX
   ```
   The script fails loudly if any `AIRLINES` keyword matches zero or more than
   one source file. Repeat for each device that should carry logos. Logos count
   against the logos partition's ~148 KB of free headroom (see the partition
   table), not the app slots.

## Settings & status indicators

Settings are stored in NVS under the `"flight"` namespace (see `setup()` in
`cyd-dashboard.ino` for the load, and the Reset confirmation in
`handleTouch` for the wipe). Notable keys:

| Key | Type | Default | Meaning |
|---|---|---|---|
| `timer` | bool | `false` | Show the dashboard countdown/timer bar (General → Enable timer). |
| `clkcol` | uint32 | `TFT_BLUE` | Dashboard clock-bar color (General → Clock Color). |

`prefs.clear()` in the Reset handler removes **all** keys for both "All" and
"Settings" resets (the "Settings" reset only re-writes the four touch-
calibration keys afterwards), so the clock color reverts to its default after
either reset.

The dashboard draws a colored screen border and tints the clock bar to flag
state (`drawAuthBorder` / `drawStatusBorder` in `cyd-dashboard.ino`):

- **Red** (critical): no WiFi, invalid OpenSky credentials, flight credits
  exhausted, or pool data unavailable. The clock bar turns maroon to match.
- **Yellow** (warning): running OpenSky **anonymously** (no credentials
  configured). This is non-critical — flights still work at a lower rate limit
  — so the clock bar keeps its normal (user-selected) color. Anonymous is only
  flagged after the first real OpenSky fetch (`g_authChecked`).

`dashboardCriticalLabel()` and `dashboardWarningLabel()` decide the two cases;
the clock bar color is `g_clockCol` (persisted `clkcol`) except when a critical
issue overrides it with maroon.

