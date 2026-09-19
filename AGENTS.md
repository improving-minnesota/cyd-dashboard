# AGENTS.md

Rules for agents working in this repo. Full details live in
[DEVELOPER.md](DEVELOPER.md); this file is the short version of what bites
people.

## Git

- All changes land via PRs to `main` — never commit to `main` directly.
- PR titles must be conventional commits (`feat:`, `fix:`, `chore:`, …); they
  drive release-please versioning.
- After a release PR merges (`chore(main): release …`), resync local `main`
  and tags: `git checkout main && git pull --tags`. The merge bumps
  `.release-please-manifest.json` + `CHANGELOG.md` and CI pushes the `vX.Y.Z`
  tag — building from a stale checkout bakes the wrong `APP_VERSION`.

## Building

- Always use FQBN `esp32:esp32:jczn_2432s028r:PartitionScheme=custom` —
  on **both** compile and upload. Dropping it on upload silently flashes the
  default partition table.
- Compile with `--clean` before flashing (stale-cache objects have masked
  real changes before).
- Dev build flags (all three or local OTA won't work):
  `-DAPP_VERSION=<ver>-dev -DENABLE_LOCAL_OTA=1 -DENABLE_SERIAL_PROVISION=1`,
  output to `build/release`.
- Board variants: same FQBN plus a model flag; each has its own build dir and
  release asset (see DEVELOPER.md "Board variants"):
  - 2.8" 2432S028R — default build → `build/release`
  - 4.0" E32R40T — add `-DCYD_E32R40T=1` → `build/release-e32r40t`

## Flashing: use local OTA, not USB

- **If a dev build with the OTA flags is already running on the board**, push
  updates with `cyd-horizon/.venv/bin/python scripts/ota_push.py`
  (`--dir build/release-e32r40t` for the 4" board, or `--all` to update every
  detected board in parallel with its variant's binary).
  Do NOT run `arduino-cli upload` for iteration — it is ~80 s, holds the
  serial port, and is the wrong tool once OTA-capable firmware is up.
- Multiple boards plugged in: every build prints `[boot] board=<model>` at
  reset; `scripts/detect_boards.py` maps ports to boards and both update
  scripts use it — `ota_push.py --board e32r40t` (inferred from `--dir` too),
  `scripts/flash.py --board e32r40t` for USB uploads. Boards running firmware
  older than the marker report `unknown` — pass `--port` once.
- USB upload is for the **first** flash only (fresh board, changed OTA flags,
  or board unreachable on WiFi): `scripts/flash.py --board <model>` picks the
  port and per-board baud (115200 for 2432s028r, 460800 for the E32R40T's
  CH340 — it drops at 921600). Always the same custom-partition FQBN.
- Never flash `*.merged.bin` at `0x0` for a routine update — it wipes NVS
  (credentials, calibration) and the LittleFS partitions.
- Serial console: `cyd-horizon/.venv/bin/python scripts/serial_monitor.py`
  (`--reset` for a clean boot log). One process holds the port at a time.

## Provisioning

- Credentials: `cyd-horizon/.venv/bin/python scripts/provision_config.py --port /dev/cu.usbserial-XXXX`
  (reads `cyd-horizon/.env`, git-ignored).
- Airline logos partition:
  `cyd-horizon/.venv/bin/python scripts/provision_logos.py`
  (flashes every detected board; `--port` targets just one).

## Docs sync

- User-facing changes must update three places as appropriate: `README.md`, the
  on-device Help screen (`kHelpLines[]` in `cyd-horizon/settings.ino`), and
  `docs/user-guide/DEVELOPER-USERGUIDE.md` (then regenerate the PDF).

## Comments

- Say **why**, not what; use short section-header comments (`// ---- X ----`)
  to organize long functions. Document non-obvious constraints, workarounds,
  and magic numbers. Don't narrate the code. See DEVELOPER.md "Code comments".
