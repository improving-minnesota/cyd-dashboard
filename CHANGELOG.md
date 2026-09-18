# Changelog

## [3.5.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v3.4.0...v3.5.0) (2026-09-18)


### Features

* watch-callsign wildcard and one-time alarms ([b7580b7](https://github.com/improving-minnesota/cyd-dashboard/commit/b7580b7abf2a2dac8e6a24e3066dcbfbb485f99c))
* watch-callsign wildcard and one-time alarms ([6dda895](https://github.com/improving-minnesota/cyd-dashboard/commit/6dda8958a721193c090a2333a1a65d04f12fe9a9))

## [3.4.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v3.3.0...v3.4.0) (2026-09-18)


### Features

* honor Pool Temp toggle and back off on API rate limits ([f619b2e](https://github.com/improving-minnesota/cyd-dashboard/commit/f619b2e15bfa4af32dbabd8eb99ab0d1c9ba94d2))
* honor Pool Temp toggle and back off on API rate limits ([844b143](https://github.com/improving-minnesota/cyd-dashboard/commit/844b143b7600b1bef9fe8e9271f807e5ad85b239))

## [3.3.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v3.2.0...v3.3.0) (2026-09-18)


### Features

* watch callsign matches any part of the callsign ([07f8b7b](https://github.com/improving-minnesota/cyd-dashboard/commit/07f8b7b96190d8b9576fad10b6731ce595675066))
* watch callsign matches any part of the callsign ([bc4d759](https://github.com/improving-minnesota/cyd-dashboard/commit/bc4d759569738e45b16467ccca2356aec4c69875))

## [3.2.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v3.1.1...v3.2.0) (2026-09-17)


### Features

* play 1-up jingle on first boot after a firmware upgrade ([10998d4](https://github.com/improving-minnesota/cyd-dashboard/commit/10998d4d56f14b0c7624b9691683dadc4a9a5fed))
* play 1-up jingle on first boot after a firmware upgrade ([062bd70](https://github.com/improving-minnesota/cyd-dashboard/commit/062bd70c1fd81d46672ca2840da72cf981ec6ca3))

## [3.1.1](https://github.com/improving-minnesota/cyd-dashboard/compare/v3.1.0...v3.1.1) (2026-09-17)


### Bug Fixes

* never run the daily update scan inside the sleep window ([c75f83b](https://github.com/improving-minnesota/cyd-dashboard/commit/c75f83b7b7f7fbc53eca1b6412de35a3a6e230a2))
* never run the daily update scan inside the sleep window ([da57257](https://github.com/improving-minnesota/cyd-dashboard/commit/da5725787da340014a28c58660c5301ffb1f0982))

## [3.1.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v3.0.0...v3.1.0) (2026-09-17)


### Features

* jitter daily update check and skip ±1h around alarms ([24ed3f4](https://github.com/improving-minnesota/cyd-dashboard/commit/24ed3f450d7ed1610df05abf578f2995676925f0))
* jitter daily update check and skip ±1h around alarms ([5b16064](https://github.com/improving-minnesota/cyd-dashboard/commit/5b160644f11047d3d9068e21f81d60a6764b4046))

## [3.0.0](https://github.com/improving-minnesota/cyd-dashboard/releases/tag/v3.0.0) (2026-09-17)


### ⚠ BREAKING CHANGES

* releases now publish only the board-named assets (cyd-dashboard-2432s028r.ino.bin, cyd-dashboard-e32r40t.ino.bin). Firmware old enough to poll for the bare cyd-dashboard.ino.bin asset can no longer see new releases and must be updated over USB.

### Features

* drop legacy cyd-dashboard.ino.bin release asset ([25fbb89](https://github.com/improving-minnesota/cyd-dashboard/commit/25fbb89eefc35779e6b30baea51230cdcd77ff37))
