# Changelog

## [3.1.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v3.0.0...v3.1.0) (2026-09-17)


### Features

* jitter daily update check and skip ±1h around alarms ([24ed3f4](https://github.com/improving-minnesota/cyd-dashboard/commit/24ed3f450d7ed1610df05abf578f2995676925f0))
* jitter daily update check and skip ±1h around alarms ([5b16064](https://github.com/improving-minnesota/cyd-dashboard/commit/5b160644f11047d3d9068e21f81d60a6764b4046))

## [3.0.0](https://github.com/improving-minnesota/cyd-dashboard/releases/tag/v3.0.0) (2026-09-17)


### ⚠ BREAKING CHANGES

* releases now publish only the board-named assets (cyd-dashboard-2432s028r.ino.bin, cyd-dashboard-e32r40t.ino.bin). Firmware old enough to poll for the bare cyd-dashboard.ino.bin asset can no longer see new releases and must be updated over USB.

### Features

* drop legacy cyd-dashboard.ino.bin release asset ([25fbb89](https://github.com/improving-minnesota/cyd-dashboard/commit/25fbb89eefc35779e6b30baea51230cdcd77ff37))
