# Changelog

## [3.0.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v2.2.0...v3.0.0) (2026-09-17)


### ⚠ BREAKING CHANGES

* releases now publish only the board-named assets (cyd-dashboard-2432s028r.ino.bin, cyd-dashboard-e32r40t.ino.bin). Firmware old enough to poll for the bare cyd-dashboard.ino.bin asset can no longer see new releases and must be updated over USB.

### Features

* drop legacy cyd-dashboard.ino.bin release asset ([25fbb89](https://github.com/improving-minnesota/cyd-dashboard/commit/25fbb89eefc35779e6b30baea51230cdcd77ff37))

## [2.2.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v2.1.1...v2.2.0) (2026-09-17)


### Features

* notify volume levels 1-10, adding 1-5% and 15% steps ([93c5fcd](https://github.com/improving-minnesota/cyd-dashboard/commit/93c5fcd26d099c4d61dd025d48b953807f01801d))
* notify volume levels 1-10, adding 1-5% and 15% steps ([bf06224](https://github.com/improving-minnesota/cyd-dashboard/commit/bf06224b5d2552541c198fefea5756048c6d9389))

## [2.1.1](https://github.com/improving-minnesota/cyd-dashboard/compare/v2.1.0...v2.1.1) (2026-09-17)


### Bug Fixes

* sync README and help screen with user guide PDF ([#123](https://github.com/improving-minnesota/cyd-dashboard/issues/123)) ([8eed154](https://github.com/improving-minnesota/cyd-dashboard/commit/8eed1543212c3061ccd5dc386ca4e0a8fa18435c))

## [2.1.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v2.0.0...v2.1.0) (2026-09-17)


### Features

* 49 LED+speaker notification presets, callsign notify, and notify volume ([#121](https://github.com/improving-minnesota/cyd-dashboard/issues/121)) ([5cef385](https://github.com/improving-minnesota/cyd-dashboard/commit/5cef3856549e58ed7d5dca377c104a08deba21e9))

## [2.0.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.18.0...v2.0.0) (2026-09-15)


### ⚠ BREAKING CHANGES

* release assets are now named per board. The legacy cyd-dashboard.ino.bin name remains as a copy of the 2432S028R build but is deprecated.

### Features

* add 4.0 in E32R40T variant, per-board OTA assets, and user guide refresh ([8214c66](https://github.com/improving-minnesota/cyd-dashboard/commit/8214c661cac41ebd25ee6a898f97577c13c8d21e))

## [1.18.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.17.2...v1.18.0) (2026-09-11)


### Features

* display IATA airport codes from adsb.lol route data ([2e0e5d1](https://github.com/improving-minnesota/cyd-dashboard/commit/2e0e5d1a9471ba8760eeb5f6377c3345770aa092))
* display IATA airport codes from adsb.lol route data ([81c25b9](https://github.com/improving-minnesota/cyd-dashboard/commit/81c25b9bee109001bfe7808f1cc06277c99e5a21))

## [1.17.2](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.17.1...v1.17.2) (2026-09-11)


### Bug Fixes

* adopt release-please manifest and gate publish marking on release ([aebf191](https://github.com/improving-minnesota/cyd-dashboard/commit/aebf19133592fb84e65b903c5379d5db2d874705))
* mark release PRs published only after publish and retry stuck draft releases ([b5c9bd3](https://github.com/improving-minnesota/cyd-dashboard/commit/b5c9bd34760757a267014ab57a6608bf99ef7221))

## 1.17.1 (2026-09-11)


### Bug Fixes

* only mark release-please PRs published when a release shipped ([7ca5243](https://github.com/improving-minnesota/cyd-dashboard/commit/7ca5243c6b091134643f2b7e9890e3c053f47947))
