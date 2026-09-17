# Changelog

## [3.0.0](https://github.com/improving-minnesota/cyd-dashboard/releases/tag/v3.0.0) (2026-09-17)


### ⚠ BREAKING CHANGES

* releases now publish only the board-named assets (cyd-dashboard-2432s028r.ino.bin, cyd-dashboard-e32r40t.ino.bin). Firmware old enough to poll for the bare cyd-dashboard.ino.bin asset can no longer see new releases and must be updated over USB.

### Features

* drop legacy cyd-dashboard.ino.bin release asset ([25fbb89](https://github.com/improving-minnesota/cyd-dashboard/commit/25fbb89eefc35779e6b30baea51230cdcd77ff37))
